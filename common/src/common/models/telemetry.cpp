#include "common/telemetry.hpp"
#include "common/json/access.hpp"

namespace web_htop::models {
using namespace web_htop::json;
Value SampleStatus::ToJson() const {
    Object o;
    Add(o, "name", name);
    Add(o, "state", state);
    Add(o, "error", error);
    Add(o, "duration_us", duration_us);
    return Value(std::move(o));
}
Value InterfaceMetrics::ToJson() const {
    Object o;
    Add(o, "name", name);
    Add(o, "rx_bytes", rx_bytes);
    Add(o, "tx_bytes", tx_bytes);
    Add(o, "rx_errors", rx_errors);
    Add(o, "tx_errors", tx_errors);
    Add(o, "rx_dropped", rx_dropped);
    Add(o, "tx_dropped", tx_dropped);
    Add(o, "rx_bytes_per_second", rx_bytes_per_second);
    Add(o, "tx_bytes_per_second", tx_bytes_per_second);
    return Value(std::move(o));
}
Value DiskIoMetrics::ToJson() const {
    Object o;
    Add(o, "device", device);
    Add(o, "read_bytes_per_second", read_bytes_per_second);
    Add(o, "write_bytes_per_second", write_bytes_per_second);
    Add(o, "iops", iops);
    Add(o, "busy_percent", busy_percent);
    return Value(std::move(o));
}
Value PressureMetrics::ToJson() const {
    Object o;
    Add(o, "resource", resource);
    Add(o, "some_avg10", some_avg10);
    Add(o, "some_avg60", some_avg60);
    Add(o, "some_avg300", some_avg300);
    Add(o, "full_avg10", full_avg10);
    return Value(std::move(o));
}
Value CgroupMetrics::ToJson() const {
    Object o;
    Add(o, "path", path);
    Add(o, "state", state);
    Add(o, "memory_current", memory_current);
    Add(o, "memory_max", memory_max);
    Add(o, "memory_unlimited", memory_unlimited);
    Add(o, "cpu_quota_cores", cpu_quota_cores);
    Add(o, "cpu_unlimited", cpu_unlimited);
    Add(o, "cpu_percent", cpu_percent);
    Add(o, "nr_throttled", nr_throttled);
    Add(o, "throttled_ms_per_second", throttled_ms_per_second);
    Add(o, "oom_kill", oom_kill);
    o.emplace_back("pressure", List(pressure));
    return Value(std::move(o));
}
Value TelemetryInfo::ToJson() const {
    Object o;
    Add(o, "interval_ms", interval_ms);
    Add(o, "sequence", sequence);
    Add(o, "instance_id", instance_id);
    Add(o, "boot_id", boot_id);
    Add(o, "hostname", hostname);
    Add(o, "scope", "procfs-view");
    Add(o, "collection_started_at", collection_started_at);
    Add(o, "collection_finished_at", collection_finished_at);
    Add(o, "collection_us", collection_us);
    Add(o, "skipped_ticks", skipped_ticks);
    Add(o, "process_denied", process_denied);
    Add(o, "process_vanished", process_vanished);
    Add(o, "process_malformed", process_malformed);
    Add(o, "processes_truncated", processes_truncated);
    Array ids;
    for (auto id : cpu_ids)
        ids.push_back(Make(id));
    o.emplace_back("cpu_ids", Value(std::move(ids)));
    o.emplace_back("collectors", List(collectors));
    o.emplace_back("interfaces", List(interfaces));
    o.emplace_back("disks", List(disks));
    o.emplace_back("pressure", List(pressure));
    o.emplace_back("cgroup", cgroup.ToJson());
    return Value(std::move(o));
}
namespace {
std::vector<PressureMetrics> ReadPressure(Value const& v) {
    std::vector<PressureMetrics> result;
    if (auto a = v.AsArray())
        for (auto const& p : *a)
            result.push_back({Text(p, "resource"), Number(p, "some_avg10"), Number(p, "some_avg60"),
                              Number(p, "some_avg300"), Number(p, "full_avg10")});
    return result;
}
} // namespace
TelemetryInfo TelemetryInfo::FromJson(Value const& v) {
    TelemetryInfo t;
    t.interval_ms = UInt(v, "interval_ms").value_or(1000);
    t.sequence = UInt(v, "sequence").value_or(0);
    t.instance_id = Text(v, "instance_id");
    t.boot_id = Text(v, "boot_id");
    t.hostname = Text(v, "hostname");
    t.collection_started_at = UInt(v, "collection_started_at").value_or(0);
    t.collection_finished_at = UInt(v, "collection_finished_at").value_or(0);
    t.collection_us = UInt(v, "collection_us").value_or(0);
    t.skipped_ticks = UInt(v, "skipped_ticks").value_or(0);
    t.process_denied = UInt(v, "process_denied").value_or(0);
    t.process_vanished = UInt(v, "process_vanished").value_or(0);
    t.process_malformed = UInt(v, "process_malformed").value_or(0);
    t.processes_truncated = Boolean(v, "processes_truncated");
    if (auto f = Field(v, "cpu_ids"))
        if (auto a = f->AsArray())
            for (auto const& id : *a)
                if (auto n = id.AsUInt64(); n && *n <= std::numeric_limits<unsigned>::max())
                    t.cpu_ids.push_back(static_cast<unsigned>(*n));
    if (auto f = Field(v, "collectors"))
        if (auto a = f->AsArray())
            for (auto const& p : *a)
                t.collectors.push_back({Text(p, "name"), Text(p, "state"), Text(p, "error"),
                                        UInt(p, "duration_us").value_or(0)});
    if (auto f = Field(v, "interfaces"))
        if (auto a = f->AsArray())
            for (auto const& p : *a)
                t.interfaces.push_back(
                    {Text(p, "name"), UInt(p, "rx_bytes").value_or(0),
                     UInt(p, "tx_bytes").value_or(0), UInt(p, "rx_errors").value_or(0),
                     UInt(p, "tx_errors").value_or(0), UInt(p, "rx_dropped").value_or(0),
                     UInt(p, "tx_dropped").value_or(0), Number(p, "rx_bytes_per_second"),
                     Number(p, "tx_bytes_per_second")});
    if (auto f = Field(v, "disks"))
        if (auto a = f->AsArray())
            for (auto const& p : *a)
                t.disks.push_back({Text(p, "device"), Number(p, "read_bytes_per_second"),
                                   Number(p, "write_bytes_per_second"), Number(p, "iops"),
                                   Number(p, "busy_percent")});
    if (auto f = Field(v, "pressure"))
        t.pressure = ReadPressure(*f);
    if (auto p = Field(v, "cgroup")) {
        auto& c = t.cgroup;
        c.path = Text(*p, "path");
        c.state = Text(*p, "state");
        c.memory_current = UInt(*p, "memory_current");
        c.memory_max = UInt(*p, "memory_max");
        c.memory_unlimited = Boolean(*p, "memory_unlimited");
        c.cpu_unlimited = Boolean(*p, "cpu_unlimited");
        c.cpu_quota_cores = Number(*p, "cpu_quota_cores");
        c.cpu_percent = Number(*p, "cpu_percent");
        c.nr_throttled = UInt(*p, "nr_throttled");
        c.oom_kill = UInt(*p, "oom_kill");
        c.throttled_ms_per_second = Number(*p, "throttled_ms_per_second");
        if (auto f = Field(*p, "pressure"))
            c.pressure = ReadPressure(*f);
    }
    return t;
}
} // namespace web_htop::models
