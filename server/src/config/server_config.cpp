#include "server/config/server_config.hpp"
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace web_htop::server
{
namespace
{
unsigned Number(std::string_view value)
{
    unsigned n{};
    auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), n);

    if (ec != std::errc{} || end != value.data() + value.size())
    {
        throw std::invalid_argument("invalid unsigned integer: " + std::string(value));
    }
    return n;
}
} // namespace

ServerConfig ServerConfig::FromArgs(int argc, char** argv)
{
    ServerConfig c;

    if (auto p = std::getenv("WEB_HTOP_HTTP_PORT"))
    {
        c.port = Number(p);
    }
    if (auto p = std::getenv("WEB_HTOP_STREAMING_PORT"))
    {
        c.streaming_port = Number(p);
    }
    for (int i = 1; i < argc; ++i)
    {
        std::string_view key = argv[i];

        if (key == "--include-loopback")
        {
            c.include_loopback = true;
            continue;
        }
        if (i + 1 == argc)
        {
            throw std::invalid_argument("missing value for " + std::string(key));
        }
        std::string value = argv[++i];

        if (key == "--bind")
        {
            c.bind_address = value;
        }
        else if (key == "--http-port")
        {
            c.port = Number(value);
        }
        else if (key == "--stream-port")
        {
            c.streaming_port = Number(value);
        }
        else if (key == "--interval-ms")
        {
            c.poll_interval = std::chrono::milliseconds(Number(value));
        }
        else if (key == "--request-timeout-ms")
        {
            c.request_timeout = std::chrono::milliseconds(Number(value));
        }
        else if (key == "--write-timeout-ms")
        {
            c.write_timeout = std::chrono::milliseconds(Number(value));
        }
        else if (key == "--max-clients")
        {
            c.max_clients = Number(value);
        }
        else if (key == "--max-processes")
        {
            c.max_processes = Number(value);
        }
        else if (key == "--proc-root")
        {
            c.proc_root = value;
        }
        else if (key == "--sys-root")
        {
            c.sys_root = value;
        }
        else if (key == "--mount")
        {
            c.mount_path = value;
        }
        else if (key == "--cgroup")
        {
            c.cgroup_path = value;
        }
        else
        {
            throw std::invalid_argument("unknown option: " + std::string(key));
        }
    }
    c.Validate();
    return c;
}

void ServerConfig::Validate() const
{
    if (port == 0 || port > 65535 || streaming_port == 0 || streaming_port > 65535 ||
        port == streaming_port)
    {
        throw std::invalid_argument("HTTP and stream ports must be distinct, in 1..65535");
    }
    if (poll_interval.count() < 100 || poll_interval.count() > 60000)
    {
        throw std::invalid_argument("interval must be 100..60000 ms");
    }
    if (request_timeout.count() < 100 || request_timeout.count() > 60000 ||
        write_timeout.count() < 100 || write_timeout.count() > 60000)
    {
        throw std::invalid_argument("timeouts must be 100..60000 ms");
    }
    if (max_clients < 1 || max_clients > 4096 || max_processes < 1 || max_processes > 10000)
    {
        throw std::invalid_argument("max-clients must be 1..4096; max-processes 1..10000");
    }
    if (bind_address.empty() || proc_root.empty() || sys_root.empty() || mount_path.empty())
    {
        throw std::invalid_argument("paths and bind address cannot be empty");
    }
}
} // namespace web_htop::server
