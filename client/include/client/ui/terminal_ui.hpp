/**
 * @file client/ui/terminal_ui.hpp
 * @brief Dense terminal workspace for live telemetry and recorded sessions.
 */
#pragma once
#include "common/models/system_snapshot.hpp"
#include <deque>
#include <string>
#include <termios.h>

namespace web_htop::client::ui
{
struct ViewState
{
    models::SystemSnapshot const* snapshot{};
    json::utils::JSONValue const* diagnostics{};
    std::deque<double> cpu_history, memory_history, rx_history;
    std::string connection, filter, sort{"cpu"};
    double age_seconds{-1}, diagnostics_age{-1};
    std::uint64_t bytes_received{}, reconnects{}, sequence_gaps{};
    unsigned page{1};
    std::size_t scroll{};
    bool paused{}, editing_filter{}, help{}, recording{};
};

[[nodiscard]] std::string Sanitize(std::string_view text);
[[nodiscard]] std::string Render(ViewState const& state, int width, int height, bool ansi = true);

class TerminalUi
{
  public:
    TerminalUi();
    ~TerminalUi();
    TerminalUi(TerminalUi const&) = delete;
    TerminalUi& operator=(TerminalUi const&) = delete;
    void Draw(ViewState const& state) const;

    [[nodiscard]] bool Interactive() const noexcept
    {
        return interactive_;
    }

  private:
    termios saved_{};
    bool interactive_{};
};
} // namespace web_htop::client::ui
