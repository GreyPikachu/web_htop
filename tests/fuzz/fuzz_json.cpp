#include "common/json/parser.hpp"
#include "common/protocol.hpp"
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) {
    std::string_view text(reinterpret_cast<char const*>(data), size);
    if (auto parsed = web_htop::json::Parse(text))
        (void)parsed->value.ToString();
    std::string error;
    (void)web_htop::protocol::Decode(text, error);
    return 0;
}
