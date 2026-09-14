#include "server/app/server_app.hpp"
#include "common/unique_fd.hpp"
#include "server/collectors/metrics_collector.hpp"
#include "server/transport/reactor.hpp"
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <pthread.h>
#include <random>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <system_error>
#include <thread>

namespace web_htop::server
{
namespace
{
class SignalMask
{
  public:
    SignalMask()
    {
        ::sigemptyset(&mask_);
        ::sigaddset(&mask_, SIGINT);
        ::sigaddset(&mask_, SIGTERM);
        int ec = ::pthread_sigmask(SIG_BLOCK, &mask_, &previous_);

        if (ec)
        {
            throw std::system_error(ec, std::generic_category(), "pthread_sigmask");
        }
    }

    ~SignalMask()
    {
        (void)::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }

    sigset_t const* Get() const
    {
        return &mask_;
    }

  private:
    sigset_t mask_{}, previous_{};
};
} // namespace

int ServerApp::Run()
{
    SignalMask mask;
    UniqueFd signals(::signalfd(-1, mask.Get(), SFD_NONBLOCK | SFD_CLOEXEC));
    UniqueFd notification(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));

    if (!signals || !notification)
    {
        throw std::system_error(errno, std::generic_category(), "control descriptor");
    }
    SharedState state;
    Reactor reactor(config_, state, signals.Get(),
                    notification.Get()); // Bind both ports before starting the worker.
    auto source = std::make_shared<system::LinuxSource>(config_);
    collectors::MetricsCollector collector(config_, source);
    std::random_device random;
    const std::string instance = std::to_string(random()) + "-" + std::to_string(random());
    std::jthread worker(
        [&](std::stop_token stop)
        {
            std::condition_variable_any wake;
            std::mutex mutex;
            auto next = std::chrono::steady_clock::now();
            std::uint64_t sequence = 0, skipped = 0;

            while (!stop.stop_requested())
            {
                try
                {
                    auto snapshot = collector.Collect(stop);

                    if (stop.stop_requested())
                    {
                        break;
                    }
                    snapshot.telemetry.sequence = ++sequence;
                    snapshot.telemetry.instance_id = instance;
                    snapshot.telemetry.skipped_ticks = skipped;
                    state.Publish(std::move(snapshot));
                    std::uint64_t one = 1;

                    while (::write(notification.Get(), &one, sizeof(one)) < 0 && errno == EINTR)
                    {
                    }
                }
                catch (std::exception const& e)
                {
                    std::cerr << "collector: generation not published: " << e.what() << '\n';
                }
                next += config_.poll_interval;
                const auto now = std::chrono::steady_clock::now();

                if (next <= now)
                {
                    auto missed = (now - next) / config_.poll_interval + 1;
                    skipped += static_cast<std::uint64_t>(missed);
                    next += missed * config_.poll_interval;
                }
                std::unique_lock lock(mutex);
                wake.wait_until(lock, stop, next,
                                []
                                {
                                    return false;
                                });
            }
        });
    std::cout << "web_htop: bind=" << config_.bind_address << " http=" << config_.port
              << " stream=" << config_.streaming_port
              << " interval_ms=" << config_.poll_interval.count()
              << " max_clients=" << config_.max_clients << '\n'
              << std::flush;
    reactor.Run();
    worker.request_stop();
    worker.join();
    std::cout << "web_htop: stopped\n";
    return 0;
}
} // namespace web_htop::server
