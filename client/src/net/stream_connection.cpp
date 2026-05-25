#include "client/net/stream_connection.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>

namespace web_htop::client {
namespace {
using Clock = std::chrono::steady_clock;
UniqueFd Connect(Address const& a, bool& connecting) {
    UniqueFd fd(::socket(a.family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    if (!fd)
        return fd;
    if (::connect(fd.Get(), reinterpret_cast<sockaddr const*>(&a.address), a.length) == 0)
        connecting = false;
    else if (errno == EINPROGRESS)
        connecting = true;
    else
        fd.Reset();
    return fd;
}
bool CheckConnected(int fd) {
    int error = 0;
    socklen_t size = sizeof(error);
    return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0;
}
constexpr std::string_view kProbeRequest =
    "GET /diagnostics HTTP/1.1\r\nHost: web-htop\r\nConnection: close\r\n\r\n";
} // namespace
std::vector<Address> Resolve(std::string const& host, unsigned port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* raw = nullptr;
    int error = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &raw);
    if (error)
        throw std::runtime_error(std::string("resolve: ") + ::gai_strerror(error));
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> owner(raw, ::freeaddrinfo);
    std::vector<Address> result;
    for (auto p = raw; p; p = p->ai_next)
        if (p->ai_addrlen <= sizeof(sockaddr_storage)) {
            Address a;
            a.family = p->ai_family;
            a.length = p->ai_addrlen;
            std::memcpy(&a.address, p->ai_addr, p->ai_addrlen);
            result.push_back(a);
        }
    if (result.empty())
        throw std::runtime_error("no usable server address");
    return result;
}
StreamConnection::StreamConnection(std::vector<Address> a) : addresses_(std::move(a)) {
    if (addresses_.empty())
        throw std::invalid_argument("empty address list");
}
void StreamConnection::Disconnect(std::string reason) {
    socket_.Reset();
    decoder_.Reset();
    connected_ = false;
    in_frame_ = false;
    status_ = std::move(reason);
    const auto cap = std::min(10000u, 250u << std::min(attempts_, 5u));
    std::uniform_int_distribution<unsigned> jitter(cap / 2, cap);
    retry_ = Clock::now() + std::chrono::milliseconds(jitter(random_));
    attempts_ = std::min(attempts_ + 1, 16u);
    ++reconnects_;
}
void StreamConnection::Tick(Clock::time_point now) {
    if (socket_) {
        if (!connected_ && now >= deadline_)
            Disconnect("connect timeout");
        else if (in_frame_ && now - frame_started_ > std::chrono::seconds(10))
            Disconnect("frame deadline exceeded");
        else if (connected_ && now - last_frame_ > std::chrono::seconds(180))
            Disconnect("stream idle timeout");
        return;
    }
    if (now < retry_)
        return;
    bool connecting = false;
    socket_ = Connect(addresses_[address_index_++ % addresses_.size()], connecting);
    if (!socket_) {
        Disconnect("connect failed; retrying");
        return;
    }
    connected_ = !connecting;
    deadline_ = now + std::chrono::seconds(3);
    last_frame_ = now;
    status_ = connected_ ? "connected" : "connecting";
}
short StreamConnection::Events() const noexcept {
    return connected_ ? POLLIN : POLLOUT;
}
void StreamConnection::Handle(short events, Receiver const& receive) {
    if (!socket_)
        return;
    if (events & POLLNVAL) {
        Disconnect("invalid socket");
        return;
    }
    if (!connected_) {
        if (!(events & (POLLOUT | POLLERR | POLLHUP)))
            return;
        if (!CheckConnected(socket_.Get())) {
            Disconnect("connect refused; retrying");
            return;
        }
        connected_ = true;
        status_ = "connected";
        last_frame_ = Clock::now();
    }
    if (!(events & (POLLIN | POLLHUP | POLLERR)))
        return;
    std::array<char, 16384> buffer{};
    for (unsigned reads = 0; reads < 16; ++reads) {
        auto n = ::recv(socket_.Get(), buffer.data(), buffer.size(), 0);
        if (n == 0) {
            Disconnect(in_frame_ ? "truncated frame" : "server closed stream");
            return;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            Disconnect("receive failed");
            return;
        }
        bytes_ += static_cast<std::uint64_t>(n);
        std::size_t offset = 0;
        try {
            while (offset < static_cast<std::size_t>(n)) {
                if (!in_frame_) {
                    in_frame_ = true;
                    frame_started_ = Clock::now();
                }
                offset += decoder_.Feed(std::span<char const>(
                    buffer.data() + offset, static_cast<std::size_t>(n) - offset));
                if (decoder_.Complete()) {
                    std::string error;
                    auto snapshot = protocol::Decode(decoder_.Payload(), error);
                    if (!snapshot) {
                        Disconnect(error);
                        return;
                    }
                    receive(std::move(*snapshot));
                    decoder_.Reset();
                    in_frame_ = false;
                    last_frame_ = Clock::now();
                    attempts_ = 0;
                }
            }
        } catch (std::length_error const& e) {
            Disconnect(e.what());
            return;
        }
    }
}
void HttpProbe::Tick() {
    auto now = Clock::now();
    if (socket_) {
        if (now >= deadline_)
            Finish(false);
        return;
    }
    if (now < retry_ || addresses_.empty())
        return;
    socket_ = Connect(addresses_[address_index_++ % addresses_.size()], connecting_);
    if (!socket_) {
        Finish(false);
        return;
    }
    writing_ = true;
    sent_ = 0;
    input_.clear();
    deadline_ = now + std::chrono::seconds(2);
}
short HttpProbe::Events() const noexcept {
    return connecting_ || writing_ ? POLLOUT : POLLIN;
}
void HttpProbe::Finish(bool success) {
    if (success) {
        auto split = input_.find("\r\n\r\n");
        if (split != std::string::npos && input_.starts_with("HTTP/1.1 200 ")) {
            auto parsed = json::Parse(std::string_view(input_).substr(split + 4));
            if (parsed && parsed->value.IsObject()) {
                value_ = std::move(parsed);
                updated_ = Clock::now();
            }
        }
    }
    socket_.Reset();
    retry_ = Clock::now() + std::chrono::seconds(2);
}
void HttpProbe::Handle(short events) {
    if (!socket_)
        return;
    if (events & (POLLERR | POLLNVAL)) {
        Finish(false);
        return;
    }
    if (connecting_) {
        if (!CheckConnected(socket_.Get())) {
            Finish(false);
            return;
        }
        connecting_ = false;
    }
    if (writing_ && events & POLLOUT) {
        auto n = ::send(socket_.Get(), kProbeRequest.data() + sent_, kProbeRequest.size() - sent_,
                        MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            Finish(false);
            return;
        }
        sent_ += static_cast<std::size_t>(n);
        if (sent_ == kProbeRequest.size())
            writing_ = false;
    }
    if (!writing_ && events & (POLLIN | POLLHUP)) {
        char buffer[4096];
        for (unsigned i = 0; i < 16; ++i) {
            auto n = ::recv(socket_.Get(), buffer, sizeof(buffer), 0);
            if (n == 0) {
                Finish(true);
                return;
            }
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return;
                Finish(false);
                return;
            }
            input_.append(buffer, static_cast<std::size_t>(n));
            if (input_.size() > 65536) {
                Finish(false);
                return;
            }
        }
    }
}
double HttpProbe::AgeSeconds() const {
    return value_ ? std::chrono::duration<double>(Clock::now() - updated_).count() : -1;
}
} // namespace web_htop::client
