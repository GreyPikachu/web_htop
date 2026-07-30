/**
 * @file server/system/linux_source.hpp
 * @brief Linux reads behind an injectable boundary; parsers never open files.
 */
#pragma once
#include "server/config/server_config.hpp"
#include <cstdint>
#include <string>
#include <sys/statvfs.h>
#include <vector>

namespace web_htop::server::system {
struct FileResult {
    std::string text;
    int error{};
    [[nodiscard]] explicit operator bool() const noexcept {
        return error == 0;
    }
};
struct PidList {
    std::vector<int> pids;
    int error{};
};
class LinuxSource {
  public:
    explicit LinuxSource(ServerConfig config) : config_(std::move(config)) {}
    virtual ~LinuxSource() = default;
    [[nodiscard]] virtual FileResult Proc(std::string const& relative) const;
    [[nodiscard]] virtual FileResult Sys(std::string const& relative) const;
    [[nodiscard]] virtual FileResult Cgroup(std::string const& relative) const;
    [[nodiscard]] virtual PidList Pids() const;
    [[nodiscard]] virtual int Filesystem(struct statvfs& result) const;
    [[nodiscard]] virtual std::string CgroupIdentity() const;

  private:
    ServerConfig config_;
};
} // namespace web_htop::server::system
