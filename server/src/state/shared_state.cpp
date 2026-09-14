#include "server/state/shared_state.hpp"
#include "common/json/access.hpp"

namespace web_htop::server
{
void SharedState::Publish(models::SystemSnapshot snapshot)
{
    auto next = std::make_shared<PublishedSnapshot>();
    auto begin = std::chrono::steady_clock::now();
    next->snapshot = std::move(snapshot);
    next->json = std::make_shared<std::string const>(protocol::Encode(next->snapshot));
    next->frame = std::make_shared<std::string const>(protocol::Frame(*next->json));
    auto processes = next->snapshot.process.ToJson();
    json::Add(*processes.AsObject(), "sequence", next->snapshot.telemetry.sequence);
    json::Add(*processes.AsObject(), "instance_id", next->snapshot.telemetry.instance_id);
    json::Add(*processes.AsObject(), "truncated", next->snapshot.telemetry.processes_truncated);
    next->processes_json = std::make_shared<std::string const>(processes.ToString());
    next->published_at = std::chrono::steady_clock::now();
    next->encode_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(next->published_at - begin).count());
    latest_.store(std::move(next), std::memory_order_release);
}
} // namespace web_htop::server
