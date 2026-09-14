/**
 * @file server/transport/reactor.hpp
 * @brief One owner for listeners, sessions, deadlines and shutdown.
 */
#pragma once
#include "common/unique_fd.hpp"
#include "server/config/server_config.hpp"
#include "server/state/shared_state.hpp"
#include "server/transport/output_queue.hpp"
#include <unordered_map>

namespace web_htop::server
{
class Reactor
{
  public:
    Reactor(ServerConfig config, SharedState const& state, int signal_fd, int notification_fd);
    void Run();

  private:
    using Clock = std::chrono::steady_clock;

    struct Session
    {
        UniqueFd fd;
        bool streaming{};
        bool responding{};
        std::string request;
        OutputQueue output;
        Clock::time_point accepted_at, last_progress;
    };

    void Register(int fd, std::uint64_t token, std::uint32_t events);
    void Modify(std::uint64_t token, Session const& session);
    void Accept(bool streaming);
    bool Read(Session& session);
    bool Write(Session& session);
    void Close(std::uint64_t token);
    void Broadcast();
    void Sweep();
    bool Enqueue(Session& session, OutputQueue::Buffer buffer);
    std::string Route(std::string_view request);
    std::string Diagnostics(bool prometheus) const;
    ServerConfig config_;
    SharedState const& state_;
    int signal_fd_, notification_fd_;
    UniqueFd epoll_, http_listener_, stream_listener_, timer_;
    std::unordered_map<std::uint64_t, Session> sessions_;
    std::uint64_t next_token_{16}, last_sequence_{}, accepted_{}, rejected_{}, dropped_{},
        timeouts_{}, bytes_sent_{};
    std::size_t queued_bytes_{};
    bool listeners_paused_{};
    Clock::time_point started_{Clock::now()};
};
} // namespace web_htop::server
