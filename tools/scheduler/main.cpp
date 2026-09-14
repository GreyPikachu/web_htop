/**
 * @file tools/scheduler/main.cpp
 * @brief Optional libbpf reader. Emits cumulative histograms as JSONL.
 */
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
volatile std::sig_atomic_t stopped = 0;

void Stop(int)
{
    stopped = 1;
}

struct ObjectCloser
{
    void operator()(bpf_object* p) const
    {
        bpf_object__close(p);
    }
};

struct LinkCloser
{
    void operator()(bpf_link* p) const
    {
        bpf_link__destroy(p);
    }
};

using Object = std::unique_ptr<bpf_object, ObjectCloser>;
using Link = std::unique_ptr<bpf_link, LinkCloser>;

std::uint64_t ReadCounter(int fd, std::uint32_t key, int cpus)
{
    std::vector<std::uint64_t> values(static_cast<std::size_t>(cpus));

    if (bpf_map_lookup_elem(fd, &key, values.data()) != 0)
    {
        throw std::runtime_error("BPF map read failed");
    }
    std::uint64_t total = 0;

    for (auto n : values)
    {
        total += n;
    }
    return total;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: web_htop_sched /path/to/runqlat.bpf.o\n";
        return 1;
    }
    try
    {
        auto raw = bpf_object__open_file(argv[1], nullptr);

        if (auto error = libbpf_get_error(raw); error)
        {
            throw std::runtime_error("BPF object open failed: " + std::to_string(error));
        }
        Object object(raw);

        if (bpf_object__load(object.get()) != 0)
        {
            throw std::runtime_error("BPF load failed; check privileges, BTF and verifier log");
        }
        std::vector<Link> links;
        bpf_program* program = nullptr;
        bpf_object__for_each_program(program, object.get())
        {
            auto link = bpf_program__attach(program);

            if (auto error = libbpf_get_error(link); error)
            {
                throw std::runtime_error("BPF attach failed: " + std::to_string(error));
            }
            links.emplace_back(link);
        }
        int histogram = bpf_object__find_map_fd_by_name(object.get(), "histogram");
        int counters = bpf_object__find_map_fd_by_name(object.get(), "counters");
        int cpus = libbpf_num_possible_cpus();

        if (histogram < 0 || counters < 0 || cpus <= 0)
        {
            throw std::runtime_error("BPF maps/topology unavailable");
        }
        std::signal(SIGINT, Stop);
        std::signal(SIGTERM, Stop);

        while (!stopped)
        {
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
            std::cout << "{\"type\":\"scheduler_histogram\",\"scope\":\"system\",\"timestamp\":"
                      << timestamp << ",\"samples\":" << ReadCounter(counters, 0, cpus)
                      << ",\"unmatched_switches\":" << ReadCounter(counters, 1, cpus)
                      << ",\"update_failures\":" << ReadCounter(counters, 2, cpus)
                      << ",\"log2_us_buckets\":[";

            for (std::uint32_t bucket = 0; bucket < 64; ++bucket)
            {
                if (bucket)
                {
                    std::cout << ',';
                }
                std::cout << ReadCounter(histogram, bucket, cpus);
            }
            std::cout << "]}\n" << std::flush;

            for (int i = 0; i < 10 && !stopped; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return 0;
    }
    catch (std::exception const& e)
    {
        std::cerr << "web_htop_sched: " << e.what() << '\n';
        return 1;
    }
}
