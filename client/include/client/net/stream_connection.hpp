/**
 * @file client/net/stream_connection.hpp
 * @brief Nonblocking client state machines driven by the UI's poll loop.
 */
#pragma once
#include "common/json/parser.hpp"
#include "common/protocol.hpp"
#include "common/unique_fd.hpp"
#include <chrono>
#include <functional>
#include <random>
#include <sys/socket.h>
#include <vector>

namespace web_htop::client {
struct Address {
    sockaddr_storage address{};
    socklen_t length{};
    int family{};
};
[[nodiscard]] std::vector<Address> Resolve(std::string const& host, unsigned port);
class StreamConnection {
  public:
    using Clock = std::chrono::steady_clock;
    using Receiver = std::function<void(models::SystemSnapshot)>;
    explicit StreamConnection(std::vector<Address> addresses);
    void Tick(Clock::time_point now);
    void Handle(short events, Receiver const& receive);
    [[nodiscard]] int Fd() const noexcept {
        return socket_.Get();
    }
    [[nodiscard]] short Events() const noexcept;
    [[nodiscard]] std::string const& Status() const noexcept {
        return status_;
    }
    [[nodiscard]] bool Connected() const noexcept {
        return connected_;
    }
    [[nodiscard]] std::uint64_t Bytes() const noexcept {
        return bytes_;
    }
    [[nodiscard]] std::uint64_t Reconnects() const noexcept {
        return reconnects_;
    }

  private:
    void Disconnect(std::string reason);
    std::vector<Address> addresses_;
    UniqueFd socket_;
    protocol::FrameDecoder decoder_;
    std::mt19937 random_{std::random_device{}()};
    Clock::time_point retry_{}, deadline_{}, last_frame_{}, frame_started_{};
    std::size_t address_index_{};
    unsigned attempts_{};
    bool connected_{}, in_frame_{};
    std::string status_{"connecting"};
    std::uint64_t bytes_{}, reconnects_{};
};
class HttpProbe {
  public:
    explicit HttpProbe(std::vector<Address> addresses) : addresses_(std::move(addresses)) {}
    void Tick();
    void Handle(short events);
    [[nodiscard]] int Fd() const noexcept {
        return socket_.Get();
    }
    [[nodiscard]] short Events() const noexcept;
    [[nodiscard]] json::utils::JSONValue const* Value() const {
        return value_ ? &value_->value : nullptr;
    }
    [[nodiscard]] double AgeSeconds() const;

  private:
    void Finish(bool success);
    std::vector<Address> addresses_;
    UniqueFd socket_;
    bool connecting_{}, writing_{};
    std::size_t sent_{}, address_index_{};
    std::string input_;
    std::optional<json::ParseResult> value_;
    std::chrono::steady_clock::time_point retry_{}, deadline_{}, updated_{};
};
} // namespace web_htop::client
