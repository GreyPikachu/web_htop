#include "server/app/server_app.hpp"
#include <iostream>
#include <string_view>
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "web_htop_server [--bind 127.0.0.1] [--http-port 8080] [--stream-port 9999]\n"
                     "  --interval-ms 1000 --max-clients 256 --max-processes 1024\n"
                     "  --request-timeout-ms 3000 --write-timeout-ms 5000\n"
                     "  --proc-root /proc --sys-root /sys --mount / --cgroup /sys/fs/cgroup/PATH\n"
                     "  --include-loopback\n";
        return 0;
    }
    try {
        return web_htop::server::ServerApp(web_htop::server::ServerConfig::FromArgs(argc, argv))
            .Run();
    } catch (std::exception const& e) {
        std::cerr << "web_htop: " << e.what() << '\n';
        return 1;
    }
}
