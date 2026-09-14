#include "client/app/client_app.hpp"
#include <charconv>
#include <iostream>
#include <string_view>

namespace
{
unsigned Port(std::string_view s)
{
    unsigned n{};
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n);

    if (ec != std::errc{} || p != s.data() + s.size() || n == 0 || n > 65535)
    {
        throw std::invalid_argument("port must be 1..65535");
    }
    return n;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        web_htop::client::ClientOptions options;
        unsigned positional = 0;

        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];

            if (arg == "--help")
            {
                std::cout
                    << "web_htop_client [host] [stream_port] [http_port] [--once] [--record FILE]\n"
                       "web_htop_client --replay FILE [--once]\n";
                return 0;
            }
            if (arg == "--once")
            {
                options.once = true;
            }
            else if (arg == "--record" || arg == "--replay")
            {
                if (i + 1 == argc)
                {
                    throw std::invalid_argument("missing file path");
                }
                (arg == "--record" ? options.record_path : options.replay_path) = argv[++i];
            }
            else if (arg.starts_with('-'))
            {
                throw std::invalid_argument("unknown option");
            }
            else if (positional++ == 0)
            {
                options.host = arg;
            }
            else if (positional == 2)
            {
                options.stream_port = Port(arg);
            }
            else if (positional == 3)
            {
                options.http_port = Port(arg);
            }
            else
            {
                throw std::invalid_argument("too many arguments");
            }
        }
        if (!options.record_path.empty() && !options.replay_path.empty())
        {
            throw std::invalid_argument("record and replay are mutually exclusive");
        }
        return web_htop::client::ClientApp(std::move(options)).Run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "web_htop_client: " << e.what() << '\n';
        return 1;
    }
}
