/** @file server/app/server_app.hpp
 *  @brief Owns the sampling worker and the foreground I/O loop.
 */
#pragma once
#include "server/config/server_config.hpp"
namespace web_htop::server {
class ServerApp {
  public:
    explicit ServerApp(ServerConfig config) : config_(std::move(config)) {}
    int Run();

  private:
    ServerConfig config_;
};
} // namespace web_htop::server
