#include "common/protocol.hpp"
#include "common/json/access.hpp"
#include "common/json/parser.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace web_htop::protocol
{
std::string Encode(models::SystemSnapshot const& snapshot)
{
    auto value = snapshot.ToJson();
    json::Add(*value.AsObject(), "protocol_version", kVersion);
    json::Add(*value.AsObject(), "type", "snapshot");
    return value.ToString();
}

std::string Frame(std::string_view payload)
{
    if (payload.empty() || payload.size() > kMaxPayload)
    {
        throw std::length_error("snapshot exceeds frame limit");
    }
    std::string out(4, '\0');
    auto n = static_cast<std::uint32_t>(payload.size());

    for (unsigned i = 0; i < 4; ++i)
    {
        out[i] = static_cast<char>((n >> (24 - 8 * i)) & 0xff);
    }
    out.append(payload);
    return out;
}

std::size_t FrameDecoder::Feed(std::span<char const> bytes)
{
    std::size_t consumed = 0;

    while (header_bytes_ < 4 && consumed < bytes.size())
    {
        expected_ = (expected_ << 8) | static_cast<unsigned char>(bytes[consumed++]);
        ++header_bytes_;

        if (header_bytes_ == 4)
        {
            if (expected_ == 0 || expected_ > kMaxPayload)
            {
                throw std::length_error("invalid frame length");
            }
            payload_.reserve(expected_);
        }
    }
    if (header_bytes_ == 4)
    {
        auto count = std::min(bytes.size() - consumed,
                              static_cast<std::size_t>(expected_) - payload_.size());

        if (count)
        {
            payload_.append(bytes.data() + consumed, count);
        }
        consumed += count;
    }
    return consumed;
}

void FrameDecoder::Reset()
{
    expected_ = 0;
    header_bytes_ = 0;
    payload_.clear();
}

std::optional<models::SystemSnapshot> Decode(std::string_view payload, std::string& error)
{
    auto fail = [&](std::string reason) -> std::optional<models::SystemSnapshot>
    {
        error = std::move(reason);
        return std::nullopt;
    };
    auto doc = json::Parse(payload);

    if (!doc)
    {
        return fail("invalid JSON or parser limit exceeded");
    }
    auto const& v = doc->value;

    if (json::UInt(v, "protocol_version") != kVersion || json::Text(v, "type") != "snapshot")
    {
        return fail("unsupported protocol/version");
    }
    for (auto key : {"cpu", "memory", "network", "disk", "process", "loadavg", "telemetry"})
    {
        if (auto p = json::Field(v, key); !p || !p->IsObject())
        {
            return fail(std::string("missing object: ") + key);
        }
    }
    for (auto key : {"cpu", "memory", "network", "disk", "process", "loadavg"})
    {
        if (!json::UInt(*json::Field(v, key), "timestamp"))
        {
            return fail(std::string("invalid timestamp: ") + key);
        }
    }
    auto const& cpu = *json::Field(v, "cpu");
    auto cores = json::UInt(cpu, "core_count");
    auto per_core = json::Field(cpu, "per_core_usage_percent");

    if (!cores || *cores > 65536 || !per_core || !per_core->IsArray() || per_core->size() != *cores)
    {
        return fail("invalid CPU topology");
    }
    auto cpu_percent = json::Number(cpu, "total_usage_percent");

    if (!cpu_percent || *cpu_percent < 0 || *cpu_percent > 100)
    {
        return fail("invalid cpu.total_usage_percent");
    }
    for (auto const& n : *per_core->AsArray())
    {
        auto value = n.AsDouble();

        if (!value || !std::isfinite(*value) || *value < 0 || *value > 100)
        {
            return fail("invalid per-core usage");
        }
    }
    for (auto key : {"memory", "disk"})
    {
        auto const& section = *json::Field(v, key);
        auto total = json::UInt(section, "total_bytes"),
             available = json::UInt(section, "available_bytes"),
             used = json::UInt(section, "used_bytes");
        auto percent = json::Number(section, "used_percent");

        if (!total || !available || !used || *available > *total || *used > *total || !percent ||
            *percent < 0 || *percent > 100)
        {
            return fail(std::string("invalid byte accounting: ") + key);
        }
    }
    for (auto key : {"rx_kbps", "tx_kbps"})
    {
        auto rate = json::Number(*json::Field(v, "network"), key);

        if (!rate || *rate < 0)
        {
            return fail(std::string("invalid network rate: ") + key);
        }
    }
    for (auto key : {"load_1m", "load_5m", "load_15m"})
    {
        auto load = json::Number(*json::Field(v, "loadavg"), key);

        if (!load || *load < 0)
        {
            return fail(std::string("invalid load: ") + key);
        }
    }
    auto const& t = *json::Field(v, "telemetry");

    if (!json::UInt(t, "sequence") || json::Text(t, "instance_id").empty())
    {
        return fail("missing snapshot identity");
    }
    auto interval = json::UInt(t, "interval_ms");

    if (!interval || *interval < 100 || *interval > 60000)
    {
        return fail("invalid sampling interval");
    }
    auto const& process = *json::Field(v, "process");
    auto list = json::Field(process, "processes");

    if (!list || !list->IsArray() || list->size() > 10000)
    {
        return fail("invalid process list");
    }
    for (auto const& p : *list->AsArray())
    {
        auto pid = json::UInt(p, "pid"), threads = json::UInt(p, "thread_count");
        auto process_cpu = json::Number(p, "cpu_percent"),
             memory = json::Number(p, "memory_percent");
        auto name = json::Field(p, "name");
        auto state = json::UInt(p, "state");

        if (!pid || *pid == 0 ||
            *pid > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) || !threads ||
            *threads > std::numeric_limits<std::uint32_t>::max() || !process_cpu ||
            *process_cpu < 0 || !memory || *memory < 0 || !name || !name->IsString() || !state ||
            *state > 127 || !json::UInt(p, "memory_bytes") || !json::UInt(p, "starttime_ticks"))
        {
            return fail("invalid process fields");
        }
    }
    auto s = models::SystemSnapshot::FromJson(v);

    if (!std::isfinite(s.cpu.total_usage_percent) || s.cpu.total_usage_percent < 0 ||
        s.cpu.total_usage_percent > 100 || !std::isfinite(s.memory.used_percent) ||
        s.memory.used_percent < 0 || s.memory.used_percent > 100)
    {
        return fail("invalid metric range");
    }
    error.clear();
    return s;
}
} // namespace web_htop::protocol
