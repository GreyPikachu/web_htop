#include "client/ui/terminal_ui.hpp"
#include "common/json/access.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace web_htop::client::ui
{
std::string Sanitize(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    // Remote comm/hostname fields are data, never terminal instructions.
    // ASCII display also gives deterministic column widths for arbitrary process names.
    for (unsigned char c : text)
    {
        out += c >= 32 && c < 127 ? static_cast<char>(c) : '?';
    }
    return out;
}

namespace
{
enum Color
{
    Normal,
    Muted,
    Cyan,
    Green,
    Amber,
    Red,
    White
};

struct Cell
{
    char32_t glyph = U' ';
    Color color = Normal;
};

std::string Utf8(char32_t c)
{
    std::string out;

    if (c < 128)
    {
        out += static_cast<char>(c);
    }
    else if (c < 2048)
    {
        out += static_cast<char>(0xc0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 63));
    }
    else
    {
        out += static_cast<char>(0xe0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    }
    return out;
}

class Canvas
{
  public:
    Canvas(int w, int h) : w_(w), h_(h), cells_(static_cast<std::size_t>(w * h))
    {
    }

    void Put(int x, int y, char32_t glyph, Color color = Normal)
    {
        if (x >= 0 && x < w_ && y >= 0 && y < h_)
        {
            cells_[static_cast<std::size_t>(y * w_ + x)] = {glyph, color};
        }
    }

    void Text(int x, int y, std::string_view text, Color color = Normal, int limit = 10000)
    {
        for (unsigned char c : Sanitize(text))
        {
            if (limit-- <= 0)
            {
                break;
            }
            Put(x++, y, c, color);
        }
    }

    void Box(int x, int y, int w, int h, std::string_view title)
    {
        for (int i = 1; i < w - 1; ++i)
        {
            Put(x + i, y, U'─', Muted);
            Put(x + i, y + h - 1, U'─', Muted);
        }
        for (int i = 1; i < h - 1; ++i)
        {
            Put(x, y + i, U'│', Muted);
            Put(x + w - 1, y + i, U'│', Muted);
        }
        Put(x, y, U'┌', Muted);
        Put(x + w - 1, y, U'┐', Muted);
        Put(x, y + h - 1, U'└', Muted);
        Put(x + w - 1, y + h - 1, U'┘', Muted);
        Text(x + 2, y, title, Cyan, w - 4);
    }

    void Bar(int x, int y, int w, double percent, Color color = Cyan)
    {
        int filled = static_cast<int>(std::clamp(percent, 0.0, 100.0) * w / 100.0);

        for (int i = 0; i < w; ++i)
        {
            Put(x + i, y, i < filled ? U'━' : U'─', i < filled ? color : Muted);
        }
    }

    void Trend(int x, int y, int w, int h, std::deque<double> const& history, Color color,
               double scale = 100)
    {
        auto start = history.size() > static_cast<std::size_t>(w)
                         ? history.size() - static_cast<std::size_t>(w)
                         : 0;

        for (std::size_t i = start; i < history.size(); ++i)
        {
            if (!std::isfinite(history[i]))
            {
                continue;
            }
            double bars = std::clamp(history[i] / std::max(scale, 0.001), 0.0, 1.0) * h;

            for (int j = 0; j < h; ++j)
            {
                double part = bars - j;

                if (part > 0)
                {
                    Put(x + static_cast<int>(i - start), y + h - 1 - j,
                        static_cast<char32_t>(
                            0x2580 + std::clamp(static_cast<int>(std::ceil(part * 8)), 1, 8)),
                        color);
                }
            }
        }
    }

    std::string String(bool ansi) const
    {
        static constexpr std::array<char const*, 7> colors{
            "\033[38;2;172;185;199m", "\033[38;2;73;91;112m",  "\033[38;2;84;196;208m",
            "\033[38;2;135;192;143m", "\033[38;2;219;175;97m", "\033[38;2;227;123;128m",
            "\033[38;2;228;235;241m"};
        std::string out = ansi ? "\033[H\033[48;2;13;19;28m" : "";

        for (int y = 0; y < h_; ++y)
        {
            Color current = White;

            if (ansi)
            {
                out += colors[current];
            }
            for (int x = 0; x < w_; ++x)
            {
                auto const& cell = cells_[static_cast<std::size_t>(y * w_ + x)];

                if (ansi && current != cell.color)
                {
                    current = cell.color;
                    out += colors[current];
                }
                out += Utf8(cell.glyph);
            }
            if (y + 1 < h_)
            {
                out += ansi ? "\r\n" : "\n";
            }
        }
        if (ansi)
        {
            out += "\033[0m";
        }
        return out;
    }

  private:
    int w_, h_;
    std::vector<Cell> cells_;
};

std::string Fixed(double n, int precision = 1)
{
    std::ostringstream s;
    s << std::fixed << std::setprecision(precision) << n;
    return s.str();
}

std::string Number(std::optional<double> n, int precision = 1)
{
    return n ? Fixed(*n, precision) : "n/a";
}

std::string Bytes(double n)
{
    constexpr std::array<char const*, 5> units{"B", "KiB", "MiB", "GiB", "TiB"};
    std::size_t u = 0;

    while (n >= 1024 && u + 1 < units.size())
    {
        n /= 1024;
        ++u;
    }
    return Fixed(n, n < 10 ? 2 : 1) + " " + units[u];
}

std::string Rate(std::optional<double> n)
{
    return n ? Bytes(*n) + "/s" : "n/a";
}

std::string Count(std::optional<std::uint64_t> n)
{
    return n ? std::to_string(*n) : "n/a";
}

Color StatusColor(std::string const& status)
{
    return status == "ok" ? Green : (status == "unavailable" ? Red : Amber);
}

bool SectionOk(ViewState const& v, std::string_view name)
{
    if (!v.snapshot)
    {
        return false;
    }
    for (auto const& s : v.snapshot->telemetry.collectors)
    {
        if (s.name == name)
        {
            return s.state == "ok";
        }
    }
    return false;
}

std::string Field(json::utils::JSONValue const* v, std::string_view key)
{
    if (!v)
    {
        return "n/a";
    }
    if (auto p = json::Field(*v, key))
    {
        if (auto n = p->AsUInt64())
        {
            return std::to_string(*n);
        }
        if (auto n = p->AsInt64())
        {
            return std::to_string(*n);
        }
        if (auto b = p->AsBool())
        {
            return *b ? "true" : "false";
        }
        if (auto s = p->AsString())
        {
            return std::string(*s);
        }
    }
    return "n/a";
}

void Processes(Canvas& c, ViewState const& v, int x, int y, int w, int h)
{
    c.Box(x, y, w, h, " PROCESS EXPLORER / sort=" + v.sort + " / one core=100% ");

    if (!v.snapshot)
    {
        return;
    }
    std::vector<models::ProcessInfo const*> processes;

    for (auto const& p : v.snapshot->process.processes)
    {
        if (v.filter.empty() || Sanitize(p.name).find(v.filter) != std::string::npos ||
            std::to_string(p.pid).find(v.filter) != std::string::npos)
        {
            processes.push_back(&p);
        }
    }
    std::ranges::sort(processes,
                      [&](auto a, auto b)
                      {
                          if (v.sort == "cpu" && a->cpu_percent != b->cpu_percent)
                          {
                              return a->cpu_percent > b->cpu_percent;
                          }
                          if (v.sort == "memory" && a->memory_bytes != b->memory_bytes)
                          {
                              return a->memory_bytes > b->memory_bytes;
                          }
                          if (v.sort == "threads" && a->thread_count != b->thread_count)
                          {
                              return a->thread_count > b->thread_count;
                          }
                          return a->pid < b->pid;
                      });
    c.Text(x + 2, y + 1, "PID      CPU%     RSS          THR   ST  NAME", Muted, w - 4);
    auto offset = std::min(v.scroll, processes.empty() ? std::size_t(0) : processes.size() - 1);

    for (std::size_t i = offset; i < processes.size() && static_cast<int>(i - offset) < h - 4; ++i)
    {
        auto const& p = *processes[i];
        std::ostringstream row;
        row << std::left << std::setw(9) << p.pid << std::setw(9)
            << (p.cpu_valid ? Fixed(p.cpu_percent) : "n/a") << std::setw(13)
            << Bytes(static_cast<double>(p.memory_bytes)) << std::setw(6) << p.thread_count
            << static_cast<char>(p.state) << "   " << Sanitize(p.name);
        c.Text(x + 2, y + 2 + static_cast<int>(i - offset), row.str(),
               p.cpu_percent >= 100 ? Amber : Normal, w - 4);
    }
    auto const& s = *v.snapshot;
    c.Text(x + 2, y + h - 2,
           "visible " + std::to_string(processes.size()) + " / sampled " +
               std::to_string(s.process.total_processes) +
               (s.telemetry.processes_truncated ? " / TOP-K TRUNCATED" : "") +
               " / filter: " + v.filter,
           Muted, w - 4);
}

void Pressure(Canvas& c, std::vector<models::PressureMetrics> const& metrics, int x, int y, int w)
{
    c.Text(x, y, "RESOURCE      SOME 10s     SOME 60s     SOME 300s    FULL 10s", Muted, w);
    int row = 1;

    for (auto const& p : metrics)
    {
        std::ostringstream line;
        line << std::left << std::setw(14) << p.resource << std::setw(13) << Number(p.some_avg10)
             << std::setw(13) << Number(p.some_avg60) << std::setw(13) << Number(p.some_avg300)
             << Number(p.full_avg10);
        c.Text(x, y + row++, line.str(), p.some_avg10.value_or(0) > 10 ? Amber : Normal, w);
    }
}
} // namespace

std::string Render(ViewState const& v, int width, int height, bool ansi)
{
    width = std::clamp(width, 1, 240);
    height = std::clamp(height, 1, 100);
    Canvas c(width, height);
    const bool stale =
        v.age_seconds >
        (v.snapshot ? 3.0 * static_cast<double>(v.snapshot->telemetry.interval_ms) / 1000.0 : 3.0);
    c.Text(2, 0, "WEB_HTOP / SYSTEM TELEMETRY", White);
    const std::string health = v.paused                             ? "FROZEN VIEW"
                               : !v.snapshot                        ? "WAITING"
                               : stale                              ? "STALE"
                               : v.connection.starts_with("replay") ? "REPLAY"
                                                                    : "LIVE";
    c.Text(std::max(32, width - 30), 0, health, v.paused || stale ? Amber : Green);
    c.Text(2, 1, "1 Overview   2 Processes   3 I/O   4 Pressure   5 Transport   6 CPU", Cyan,
           width - 4);
    c.Text(2, 2,
           "[" + std::to_string(v.page) + "] " + v.connection + "  | age " +
               (v.age_seconds < 0 ? "n/a" : Fixed(v.age_seconds) + "s") + "  | " +
               (v.snapshot ? v.snapshot->telemetry.hostname : "waiting for first snapshot"),
           Muted, width - 4);

    if (width < 80 || height < 26)
    {
        c.Box(0, 4, width, height - 7, " COMPACT VIEW ");
        c.Text(2, 6, "Resize to at least 80 x 26 for the full workspace.", Amber, width - 4);

        if (v.snapshot)
        {
            c.Text(2, 8,
                   "CPU " + Fixed(v.snapshot->cpu.total_usage_percent) + "% / RAM " +
                       Fixed(v.snapshot->memory.used_percent) + "%",
                   Normal, width - 4);
            c.Text(2, 9, "Processes " + std::to_string(v.snapshot->process.total_processes), Normal,
                   width - 4);
        }
    }
    else if (v.help)
    {
        c.Box(0, 4, width, height - 7, " WORKSPACE KEYS ");
        std::vector<std::string> help{
            "1..6   switch workspace",
            "c / m / p / t   sort processes by CPU / RSS / PID / thread count",
            "/   edit process filter; Enter accepts, Esc cancels",
            "j / k   scroll process table; g returns to the first row",
            "Space   freeze the visible snapshot; acquisition and recording continue",
            "h   toggle help     q   quit",
            "CPU% uses one core = 100%. A multithreaded process can exceed 100%.",
            "n/a means there is no valid rate yet. Zero is a valid measurement.",
            "Disk rows are separate devices; stacked devices are not added together.",
            "PSI reports stalled task time, not CPU utilization.",
            "Recording: --record session.jsonl. Replay: --replay session.jsonl.",
            "Transport counters are fetched independently through /diagnostics."};

        for (std::size_t i = 0; i < help.size() && static_cast<int>(i) < height - 10; ++i)
        {
            c.Text(3, 6 + static_cast<int>(i), help[i], i < 6 ? White : Normal, width - 6);
        }
    }
    else if (!v.snapshot)
    {
        c.Box(0, 4, width, height - 7, " CONNECTION ");
        c.Text(3, 7, "No snapshot received. The UI remains responsive while reconnecting.", Amber,
               width - 6);
    }
    else
    {
        auto const& s = *v.snapshot;
        auto const& t = s.telemetry;

        if (v.page == 1)
        {
            int half = width / 2;
            c.Box(0, 4, half, 9, " CPU / 120 SAMPLES ");
            c.Text(2, 5,
                   (SectionOk(v, "cpu") ? Fixed(s.cpu.total_usage_percent) + "%" : "n/a") + "   " +
                       std::to_string(s.cpu.core_count) + " cores   load " +
                       Fixed(s.loadavg.load_1m),
                   White, half - 4);
            c.Trend(2, 7, half - 4, 4, v.cpu_history, Cyan);
            c.Box(half, 4, width - half, 9, " MEMORY / WORKING SET ");
            c.Text(half + 2, 5,
                   (SectionOk(v, "memory")
                        ? Bytes(static_cast<double>(s.memory.used_bytes)) + " / " +
                              Bytes(static_cast<double>(s.memory.total_bytes))
                        : "unavailable"),
                   White, width - half - 4);
            c.Trend(half + 2, 7, width - half - 4, 4, v.memory_history, Green);
            c.Box(0, 13, half, 6, " COLLECTION PIPELINE ");
            c.Text(2, 14,
                   "generation " + std::to_string(t.sequence) + "  collect " +
                       Fixed(static_cast<double>(t.collection_us) / 1000) + " ms",
                   Normal, half - 4);
            c.Text(2, 15,
                   "skipped ticks " + std::to_string(t.skipped_ticks) + "  denied PIDs " +
                       std::to_string(t.process_denied),
                   Muted, half - 4);
            c.Text(2, 16, "scope: procfs-view / disk capacity via statvfs", Muted, half - 4);
            c.Box(half, 13, width - half, 6, " NETWORK / AGGREGATE (NON-LOOPBACK DEFAULT) ");
            c.Text(half + 2, 14,
                   (SectionOk(v, "network") ? "RX " + Bytes(s.network.rx_kbps * 1024) + "/s   TX " +
                                                  Bytes(s.network.tx_kbps * 1024) + "/s"
                                            : "rates warming up / partial: inspect I/O workspace"),
                   Normal, width - half - 4);
            double scale = v.rx_history.empty()
                               ? 1
                               : *std::max_element(v.rx_history.begin(), v.rx_history.end());
            c.Trend(half + 2, 15, width - half - 4, 2, v.rx_history, Cyan, std::max(scale, 1.0));
            Processes(c, v, 0, 19, width, height - 22);
        }
        else if (v.page == 2)
        {
            Processes(c, v, 0, 4, width, height - 7);
        }
        else if (v.page == 3)
        {
            int nh = std::min(10, static_cast<int>(t.interfaces.size()) + 4);
            c.Box(0, 4, width, nh, " NETWORK INTERFACES / BYTES PER SECOND ");
            c.Text(2, 5,
                   "INTERFACE          RX RATE           TX RATE           RX ERR / DROP    TX ERR "
                   "/ DROP",
                   Muted, width - 4);

            for (std::size_t i = 0; i < t.interfaces.size() && static_cast<int>(i) < nh - 3; ++i)
            {
                auto const& n = t.interfaces[i];
                std::ostringstream row;
                row << std::left << std::setw(19) << n.name << std::setw(18)
                    << Rate(n.rx_bytes_per_second) << std::setw(18) << Rate(n.tx_bytes_per_second)
                    << std::setw(17)
                    << (std::to_string(n.rx_errors) + " / " + std::to_string(n.rx_dropped))
                    << n.tx_errors << " / " << n.tx_dropped;
                c.Text(2, 6 + static_cast<int>(i), row.str(), Normal, width - 4);
            }
            c.Box(0, 4 + nh, width, height - 7 - nh,
                  " BLOCK DEVICES / NO AGGREGATION ACROSS STACKED DEVICES ");
            c.Text(2, 5 + nh,
                   "DEVICE             READ              WRITE             IOPS       BUSY%", Muted,
                   width - 4);

            for (std::size_t i = v.scroll;
                 i < t.disks.size() && static_cast<int>(i - v.scroll) < height - 11 - nh; ++i)
            {
                auto const& d = t.disks[i];
                std::ostringstream row;
                row << std::left << std::setw(19) << d.device << std::setw(18)
                    << Rate(d.read_bytes_per_second) << std::setw(18)
                    << Rate(d.write_bytes_per_second) << std::setw(11) << Number(d.iops)
                    << Number(d.busy_percent);
                c.Text(2, 6 + nh + static_cast<int>(i - v.scroll), row.str(), Normal, width - 4);
            }
        }
        else if (v.page == 4)
        {
            c.Box(0, 4, width, 8, " PRESSURE STALL INFORMATION / PROCFS VIEW / PERCENT ");
            Pressure(c, t.pressure, 2, 5, width - 4);
            c.Text(2, 10, "some = at least one task stalled; full = all non-idle tasks stalled",
                   Muted, width - 4);
            c.Box(0, 12, width, height - 15, " CGROUP V2 / EXPLICIT SCOPE ");
            auto const& g = t.cgroup;
            c.Text(2, 13, "path: " + (g.path.empty() ? "disabled (server --cgroup PATH)" : g.path),
                   White, width - 4);
            c.Text(2, 14,
                   "state: " + g.state + "   CPU " + Number(g.cpu_percent) + "%   quota " +
                       (g.cpu_unlimited ? "unlimited" : Number(g.cpu_quota_cores)) + " cores",
                   Normal, width - 4);
            c.Text(2, 15,
                   "memory " +
                       (g.memory_current ? Bytes(static_cast<double>(*g.memory_current)) : "n/a") +
                       " / " +
                       (g.memory_unlimited ? "unlimited"
                        : g.memory_max     ? Bytes(static_cast<double>(*g.memory_max))
                                           : "n/a") +
                       "   OOM kills " + Count(g.oom_kill),
                   Normal, width - 4);
            c.Text(2, 16,
                   "throttled periods " + Count(g.nr_throttled) + "   throttled ms/s " +
                       Number(g.throttled_ms_per_second),
                   Amber, width - 4);
            Pressure(c, g.pressure, 2, 18, width - 4);
        }
        else if (v.page == 6)
        {
            c.Box(0, 4, width, height - 7, " CPU MATRIX / LINUX CPU IDS ");
            c.Text(2, 5,
                   "Load 1m/5m/15m: " + Fixed(s.loadavg.load_1m) + " / " +
                       Fixed(s.loadavg.load_5m) + " / " + Fixed(s.loadavg.load_15m),
                   White, width - 4);
            const int columns = std::max(1, (width - 4) / 19);
            const std::size_t begin = v.scroll * static_cast<std::size_t>(columns);

            for (std::size_t i = begin; i < s.cpu.per_core_usage_percent.size(); ++i)
            {
                const int row = static_cast<int>((i - begin) / static_cast<std::size_t>(columns));

                if (7 + row * 3 >= height - 5)
                {
                    break;
                }
                const int x =
                    2 + static_cast<int>((i - begin) % static_cast<std::size_t>(columns)) * 19;
                const int y = 7 + row * 3;
                const auto id = i < t.cpu_ids.size() ? t.cpu_ids[i] : static_cast<unsigned>(i);
                const auto usage = s.cpu.per_core_usage_percent[i];
                const auto color = usage >= 90 ? Red : usage >= 65 ? Amber : Cyan;
                c.Text(x, y, "CPU " + std::to_string(id) + "  " + Fixed(usage) + "%", color, 17);
                c.Bar(x, y + 1, 16, usage, color);
            }
            c.Text(2, height - 5, "j/k scroll rows | topology changes restart affected baselines",
                   Muted, width - 4);
        }
        else
        {
            int left = width / 2;
            c.Box(0, 4, left, height - 7, " COLLECTOR HEALTH / CURRENT GENERATION ");
            int row = 6;

            for (auto const& status : t.collectors)
            {
                c.Text(2, row, status.name, White, left - 4);
                c.Text(18, row, status.state, StatusColor(status.state), left - 20);
                c.Text(2, row + 1, std::to_string(status.duration_us) + " us " + status.error,
                       Muted, left - 4);
                row += 2;

                if (row >= height - 5)
                {
                    break;
                }
            }
            c.Box(left, 4, width - left, height - 7, " TRANSPORT / BOUNDED DELIVERY ");
            std::vector<std::pair<std::string, std::string>> stats{
                {"Received", Bytes(static_cast<double>(v.bytes_received))},
                {"Reconnects", std::to_string(v.reconnects)},
                {"Sequence gaps", std::to_string(v.sequence_gaps)},
                {"Active sessions", Field(v.diagnostics, "active_sessions")},
                {"Queued bytes", Field(v.diagnostics, "queued_bytes")},
                {"Queue budget", Field(v.diagnostics, "queue_budget_bytes")},
                {"Superseded frames", Field(v.diagnostics, "dropped_snapshots_total")},
                {"Timeouts", Field(v.diagnostics, "timeouts_total")},
                {"Rejected clients", Field(v.diagnostics, "rejected_total")},
                {"Encode us", Field(v.diagnostics, "encode_us")},
                {"Store lock-free", Field(v.diagnostics, "snapshot_store_lock_free")},
                {"Probe age", v.diagnostics_age < 0 ? "n/a" : Fixed(v.diagnostics_age) + " s"},
                {"Instance", t.instance_id}};

            for (std::size_t i = 0; i < stats.size() && static_cast<int>(i) < height - 10; ++i)
            {
                c.Text(left + 2, 6 + static_cast<int>(i), stats[i].first, Muted, 21);
                c.Text(left + 24, 6 + static_cast<int>(i), stats[i].second, Normal,
                       width - left - 26);
            }
        }
    }
    c.Text(2, height - 2,
           v.editing_filter
               ? "FILTER > " + v.filter
               : "c/m/p/t sort   / filter   j/k scroll   Space freeze   h help   q quit",
           v.editing_filter ? Amber : Muted, width - 4);
    const auto generation = v.snapshot ? std::to_string(v.snapshot->telemetry.sequence) : "n/a";
    const auto interval = v.snapshot ? std::to_string(v.snapshot->telemetry.interval_ms) : "n/a";
    c.Text(2, height - 1,
           (v.recording ? "REC  " : "") + std::string("generation ") + generation + " | interval " +
               interval + " ms | scope: procfs-view",
           Muted, width - 4);
    return c.String(ansi);
}

TerminalUi::TerminalUi()
{
    interactive_ = ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO);

    if (!interactive_)
    {
        return;
    }
    if (::tcgetattr(STDIN_FILENO, &saved_) != 0)
    {
        throw std::runtime_error("tcgetattr failed");
    }
    auto raw = saved_;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        throw std::runtime_error("tcsetattr failed");
    }
    std::cout << "\033[?1049h\033[?25l\033[2J" << std::flush;
}

TerminalUi::~TerminalUi()
{
    if (interactive_)
    {
        (void)::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
        std::cout << "\033[0m\033[?25h\033[?1049l" << std::flush;
    }
}

void TerminalUi::Draw(ViewState const& state) const
{
    if (!interactive_)
    {
        return;
    }
    winsize size{};
    (void)::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    std::cout << Render(state, size.ws_col ? size.ws_col : 120, size.ws_row ? size.ws_row : 40)
              << std::flush;
}
} // namespace web_htop::client::ui
