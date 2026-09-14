/** @file server/config/server_config.hpp
 *  @brief Validated runtime limits. No configuration is read from the network.
 */
#pragma once
#include <chrono>
#include <cstddef>
#include <string>

namespace web_htop::server
{
struct ServerConfig
{
    std::string bind_address{"127.0.0.1"};
    unsigned port{8080}, streaming_port{9999};
    std::chrono::milliseconds poll_interval{1000}, request_timeout{3000}, write_timeout{5000};
    std::size_t max_clients{256}, max_processes{1024};
    std::string proc_root{"/proc"}, sys_root{"/sys"}, mount_path{"/"}, cgroup_path;
    bool include_loopback{};
    static ServerConfig FromArgs(int argc, char** argv);
    void Validate() const;
};
} // namespace web_htop::server
