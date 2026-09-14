/**
 * @file common/json/parser.hpp
 * @brief Bounded JSON parser. Successful documents own all keys and strings.
 * @details Input is borrowed only for the duration of Parse. Syntax, depth,
 * size and allocation failures return nullopt. Maximum input is 8 MiB,
 * nesting depth 64, node count 1,000,000, object size 4,096 keys.
 */
#pragma once
#include "utils.hpp"
#include <optional>
#include <string_view>

namespace web_htop::json
{
struct ParseResult
{
    utils::JSONValue value;
};

[[nodiscard]] std::optional<ParseResult> Parse(std::string_view input);
} // namespace web_htop::json
