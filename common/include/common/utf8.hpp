/** @file common/utf8.hpp
 *  @brief Validated UTF-8 sequence boundaries for JSON strings.
 */
#pragma once
#include <string_view>
namespace web_htop {
inline unsigned Utf8Length(std::string_view text, std::size_t offset) {
    if (offset >= text.size())
        return 0;
    auto first = static_cast<unsigned char>(text[offset]);
    if (first < 128)
        return 1;
    unsigned length = first >= 0xc2 && first <= 0xdf   ? 2
                      : first >= 0xe0 && first <= 0xef ? 3
                      : first >= 0xf0 && first <= 0xf4 ? 4
                                                       : 0;
    if (!length || offset + length > text.size())
        return 0;
    for (unsigned i = 1; i < length; ++i) {
        auto byte = static_cast<unsigned char>(text[offset + i]);
        if (byte < 0x80 || byte > 0xbf)
            return 0;
    }
    auto second = static_cast<unsigned char>(text[offset + 1]);
    if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
        (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
        return 0;
    return length;
}
} // namespace web_htop
