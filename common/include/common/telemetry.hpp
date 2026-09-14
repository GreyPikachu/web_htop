/**
 * @file common/telemetry.hpp
 * @brief Sampling provenance and optional Linux telemetry sections.
 */
#pragma once
#include "common/json/utils.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace web_htop::models
{
struct SampleStatus
{
    std::string name;
    std::string state{"unavailable"}; // ok, warming_up, partial, unavailable
    std::string error;
    std::uint64_t duration_us{};
    [[nodiscard]] json::utils::JSONValue ToJson() const;
};

struct InterfaceMetrics
{
    std::string name;
    std::uint64_t rx_bytes{}, tx_bytes{}, rx_errors{}, tx_errors{}, rx_dropped{}, tx_dropped{};
    std::optional<double> rx_bytes_per_second, tx_bytes_per_second;
    [[nodiscard]] json::utils::JSONValue ToJson() const;
};

struct DiskIoMetrics
{
    std::string device;
    std::optional<double> read_bytes_per_second, write_bytes_per_second, iops, busy_percent;
    [[nodiscard]] json::utils::JSONValue ToJson() const;
};

struct PressureMetrics
{
    std::string resource;
    std::optional<double> some_avg10, some_avg60, some_avg300, full_avg10;
    [[nodiscard]] json::utils::JSONValue ToJson() const;
};

struct CgroupMetrics
{
    std::string path;
    std::string state{"disabled"};
    std::optional<std::uint64_t> memory_current, memory_max, oom_kill, nr_throttled;
    bool memory_unlimited{};
    std::optional<double> cpu_quota_cores, cpu_percent, throttled_ms_per_second;
    bool cpu_unlimited{};
    std::vector<PressureMetrics> pressure;
    [[nodiscard]] json::utils::JSONValue ToJson() const;
};

struct TelemetryInfo
{
    std::uint64_t interval_ms{1000};
    std::uint64_t sequence{}, collection_started_at{}, collection_finished_at{}, collection_us{};
    std::uint64_t skipped_ticks{}, process_denied{}, process_vanished{}, process_malformed{};
    bool processes_truncated{};
    std::string instance_id, boot_id, hostname;
    std::vector<unsigned> cpu_ids;
    std::vector<SampleStatus> collectors;
    std::vector<InterfaceMetrics> interfaces;
    std::vector<DiskIoMetrics> disks;
    std::vector<PressureMetrics> pressure;
    CgroupMetrics cgroup;
    [[nodiscard]] json::utils::JSONValue ToJson() const;
    [[nodiscard]] static TelemetryInfo FromJson(json::utils::JSONValue const& value);
};
} // namespace web_htop::models
