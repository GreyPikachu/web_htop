/**
 * @file server/state/shared_state.hpp
 * @brief One immutable generation, including its pre-encoded representations.
 */
#pragma once
#include "common/protocol.hpp"
#include <atomic>
#include <chrono>
#include <memory>

namespace web_htop::server {
struct PublishedSnapshot {
    models::SystemSnapshot snapshot;
    std::shared_ptr<std::string const> json, frame, processes_json;
    std::chrono::steady_clock::time_point published_at;
    std::uint64_t encode_us{};
};
class SharedState {
  public:
    void Publish(models::SystemSnapshot snapshot);
    [[nodiscard]] std::shared_ptr<PublishedSnapshot const> Load() const noexcept {
        return latest_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool IsLockFree() const noexcept {
        return latest_.is_lock_free();
    }

  private:
    std::atomic<std::shared_ptr<PublishedSnapshot const>> latest_;
};
} // namespace web_htop::server
