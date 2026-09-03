#include "common/protocol.hpp"
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) {
    web_htop::protocol::FrameDecoder decoder;
    try {
        for (std::size_t offset = 0; offset < size;) {
            auto n = decoder.Feed(std::span(reinterpret_cast<char const*>(data + offset),
                                            std::min<std::size_t>(size - offset, 17)));
            offset += n;
            if (decoder.Complete())
                decoder.Reset();
        }
    } catch (std::length_error const&) {
    }
    return 0;
}
