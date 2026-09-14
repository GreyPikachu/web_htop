/**
 * @file common/protocol.hpp
 * @brief Versioned JSON snapshots inside a bounded, big-endian length frame.
 */
#pragma once
#include "common/models/system_snapshot.hpp"
#include <optional>
#include <span>
#include <string>

namespace web_htop::protocol
{
inline constexpr std::uint64_t kVersion = 2;
inline constexpr std::size_t kMaxPayload = 8 * 1024 * 1024;
[[nodiscard]] std::string Encode(models::SystemSnapshot const& snapshot);
[[nodiscard]] std::string Frame(std::string_view payload);
[[nodiscard]] std::optional<models::SystemSnapshot> Decode(std::string_view payload,
                                                           std::string& error);

class FrameDecoder
{
  public:
    // Feed consumes at most one frame. Keep the unconsumed suffix for the next call.
    [[nodiscard]] std::size_t Feed(std::span<char const> bytes);

    [[nodiscard]] bool Complete() const noexcept
    {
        return expected_ != 0 && payload_.size() == expected_;
    }

    [[nodiscard]] std::string_view Payload() const noexcept
    {
        return payload_;
    }

    void Reset();

  private:
    std::uint32_t expected_{};
    unsigned header_bytes_{};
    std::string payload_;
};
} // namespace web_htop::protocol
