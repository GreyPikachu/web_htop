/**
 * @file server/collectors/metrics_collector.hpp
 * @brief Single-writer sampling pipeline. Each section carries its own validity.
 */
#pragma once
#include "common/models/system_snapshot.hpp"
#include "server/collectors/linux_samples.hpp"
#include "server/system/linux_source.hpp"
#include <chrono>
#include <memory>
#include <stop_token>
#include <unordered_map>

namespace web_htop::server::collectors
{
class MetricsCollector
{
  public:
    MetricsCollector(ServerConfig config, std::shared_ptr<system::LinuxSource const> source);
    [[nodiscard]] models::SystemSnapshot
    Collect(std::stop_token stop = {},
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

  private:
    void Cpu(models::SystemSnapshot& s);
    void Memory(models::SystemSnapshot& s);
    void Processes(models::SystemSnapshot& s, std::stop_token stop);
    void Network(models::SystemSnapshot& s);
    void Disk(models::SystemSnapshot& s);
    void Pressure(models::SystemSnapshot& s);
    void Cgroup(models::SystemSnapshot& s);
    void Load(models::SystemSnapshot& s);
    void Filesystem(models::SystemSnapshot& s);
    ServerConfig config_;
    std::shared_ptr<system::LinuxSource const> source_;
    CpuSample cpu_previous_;
    std::unordered_map<int, std::pair<std::uint64_t, std::uint64_t>> processes_previous_;

    struct NetSample
    {
        std::string identity;
        std::uint64_t rx{}, tx{};
    };

    std::map<std::string, NetSample> network_previous_;
    std::map<std::string, std::array<std::uint64_t, 5>> disks_previous_;
    std::optional<std::uint64_t> cg_usage_, cg_throttled_;
    std::string cg_identity_, boot_id_, hostname_;
    std::optional<std::chrono::steady_clock::time_point> previous_time_;
    double seconds_{};
    long clock_ticks_, page_size_;
};
} // namespace web_htop::server::collectors
