#pragma once
#include <string>

namespace web_htop::client
{
struct ClientOptions
{
    std::string host{"localhost"}, record_path, replay_path;
    unsigned stream_port{9999}, http_port{8080};
    bool once{};
};

class ClientApp
{
  public:
    explicit ClientApp(ClientOptions options) : options_(std::move(options))
    {
    }

    int Run();

  private:
    ClientOptions options_;
};
} // namespace web_htop::client
