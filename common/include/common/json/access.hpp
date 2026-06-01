/** @file common/json/access.hpp
 *  @brief Typed access helpers shared by telemetry codecs.
 */
#pragma once
#include "common/json/utils.hpp"
#include <cmath>
#include <concepts>
#include <limits>
#include <stdexcept>

namespace web_htop::json {
using Value = utils::JSONValue;
using Object = utils::JSONObject;
using Array = utils::JSONArray;
inline Value const* Field(Value const& v, std::string_view key) {
    auto f = v[key];
    return f ? &f->get() : nullptr;
}
inline std::optional<std::uint64_t> UInt(Value const& v, std::string_view key) {
    auto f = Field(v, key);
    return f ? f->AsUInt64() : std::nullopt;
}
inline std::optional<double> Number(Value const& v, std::string_view key) {
    auto f = Field(v, key);
    auto n = f ? f->AsDouble() : std::nullopt;
    return n && std::isfinite(*n) ? n : std::nullopt;
}
inline std::string Text(Value const& v, std::string_view key, std::string fallback = {}) {
    auto f = Field(v, key);
    auto s = f ? f->AsString() : std::nullopt;
    return s ? std::string(*s) : std::move(fallback);
}
inline bool Boolean(Value const& v, std::string_view key) {
    auto f = Field(v, key);
    return f && f->AsBool().value_or(false);
}
inline Value Make(std::string_view v) {
    return Value(v);
}
inline Value Make(std::string const& v) {
    return Value(std::string_view(v));
}
inline Value Make(char const* v) {
    return Value(v);
}
inline Value Make(bool v) {
    return Value(v);
}
template <std::integral T>
    requires(!std::same_as<T, bool>)
Value Make(T v) {
    if constexpr (std::is_signed_v<T>)
        return Value(static_cast<std::int64_t>(v));
    else
        return Value(static_cast<std::uint64_t>(v));
}
template <std::floating_point T> Value Make(T v) {
    return Value(static_cast<double>(v));
}
template <typename T> Value Make(std::optional<T> const& v) {
    return v ? Make(*v) : Value();
}
template <typename T> void Add(Object& o, std::string_view key, T const& v) {
    o.emplace_back(std::string(key), Make(v));
}
template <typename T> Value List(std::vector<T> const& values) {
    Array a;
    a.reserve(values.size());
    for (auto const& value : values)
        a.push_back(value.ToJson());
    return Value(std::move(a));
}
} // namespace web_htop::json
