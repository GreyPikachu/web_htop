#include "server/collectors/linux_samples.hpp"
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) {
    std::string_view text(reinterpret_cast<char const*>(data), size);
    (void)web_htop::server::collectors::ParseProcess(text);
    (void)web_htop::server::collectors::ParseCpu(text);
    return 0;
}
