#pragma once

/**
 * @file
 * @brief Normalized UTF-8 text helpers.
 */

#include <algorithm>
#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <uni_algo/conv.h>
#include <uni_algo/norm.h>

#include "expected.hpp"

namespace text {

using v1::Expected;
using v1::Unex;

struct [[nodiscard]] TextError final {
    enum class Code {
        kInvalidUtf8,
    };
    Code code;
};

class [[nodiscard]] String {
public:
    constexpr String() = default;
    constexpr String(const String &) = default;
    constexpr String(String &&) noexcept = default;
    constexpr String &operator=(const String &) = default;
    constexpr String &operator=(String &&) noexcept = default;
    ~String() = default;

    [[nodiscard]] static constexpr Expected<String, TextError> fromBytes(std::string_view bytes)
    {
        if (!una::is_valid_utf8(bytes)) {
            return Unex(
                TextError{
                    .code = TextError::Code::kInvalidUtf8,
                }
            );
        }
        String result;
        result.data = una::norm::to_nfc_utf8(bytes);
        return result;
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return {data.data(), data.size()};
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return data.empty(); }

    [[nodiscard]] constexpr size_t sizeBytes() const noexcept { return data.size(); }

    [[nodiscard]] constexpr bool startsWith(const String &prefix) const noexcept
    {
        if (prefix.data.size() > data.size())
            return false;
        const std::string_view dataPrefix{std::string_view{data}.substr(0, prefix.data.size())};
        return std::ranges::equal(prefix.data, dataPrefix);
    }

    [[nodiscard]] constexpr bool startsWith(std::string_view prefix) const noexcept
    {
        return data.starts_with(prefix);
    }

    [[nodiscard]] constexpr bool startsWith(char prefix) const noexcept
    {
        return data.starts_with(prefix);
    }

    [[nodiscard]] constexpr bool endsWith(std::string_view suffix) const noexcept
    {
        return data.ends_with(suffix);
    }

    [[nodiscard]] constexpr bool endsWith(char suffix) const noexcept
    {
        return data.ends_with(suffix);
    }

    [[nodiscard]] constexpr bool endsWith(const String &suffix) const noexcept
    {
        return data.ends_with(std::string_view{suffix.data});
    }

    constexpr String &operator+=(const String &rhs)
    {
        if (rhs.data.empty())
            return *this;
        std::string combined;
        combined.reserve(data.size() + rhs.data.size());
        combined.assign(data);
        combined += rhs.data;
        data = una::norm::to_nfc_utf8({combined.data(), combined.size()});
        return *this;
    }

    [[nodiscard]] constexpr String reversed() const
    {
        auto utf32 = una::utf8to32u(data);
        std::ranges::reverse(utf32);

        const auto reversedUtf8 = una::utf32to8(utf32);

        String result;
        result.data = una::norm::to_nfc_utf8({reversedUtf8.data(), reversedUtf8.size()});
        return result;
    }

    [[nodiscard]] friend constexpr bool operator==(const String &lhs, const String &rhs) noexcept
    {
        return lhs.data == rhs.data;
    }

    [[nodiscard]] friend constexpr bool operator<(const String &lhs, const String &rhs) noexcept
    {
        return lhs.data < rhs.data;
    }

private:
    std::string data;
};

[[nodiscard]] inline std::string toBytes(const String &text) { return std::string{text.view()}; }

namespace detail {

template <typename T> [[nodiscard]] constexpr std::string_view byteView(const T &value) noexcept
{
    return {value.data(), value.size()};
}

template <std::ranges::input_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<const Range>>
[[nodiscard]] auto collectExpected(const Range &range, F &&f)
{
    using ExpectedResult =
        std::remove_cvref_t<std::invoke_result_t<F, std::ranges::range_reference_t<const Range>>>;
    using Value = typename ExpectedResult::value_type;
    using Error = typename ExpectedResult::error_type;

    std::vector<Value> out;
    if constexpr (std::ranges::sized_range<const Range>)
        out.reserve(std::ranges::size(range));

    for (const auto &item : range) {
        auto converted = std::invoke(f, item);
        if (!converted)
            return Expected<std::vector<Value>, Error>{Unex(std::move(converted).error())};
        out.push_back(std::move(*converted));
    }
    return Expected<std::vector<Value>, Error>{std::move(out)};
}

} // namespace detail

template <std::ranges::input_range Range>
[[nodiscard]] Expected<std::vector<String>, TextError> stringVector(const Range &bytes)
{
    return detail::collectExpected(bytes, [](const auto &value) {
        return String::fromBytes(detail::byteView(value));
    });
}

template <std::ranges::input_range Range>
[[nodiscard]] Expected<std::vector<std::pair<String, String>>, TextError>
stringPairs(const Range &pairs)
{
    return detail::collectExpected(pairs, [](const auto &pair) {
        auto first = String::fromBytes(detail::byteView(pair.first));
        if (!first)
            return Expected<std::pair<String, String>, TextError>{Unex(std::move(first).error())};

        auto second = String::fromBytes(detail::byteView(pair.second));
        if (!second)
            return Expected<std::pair<String, String>, TextError>{Unex(std::move(second).error())};

        return Expected<std::pair<String, String>, TextError>{
            std::pair<String, String>{std::move(*first), std::move(*second)}
        };
    });
}

template <typename T>
[[nodiscard]] Expected<std::optional<String>, TextError>
optionalString(const std::optional<T> &bytes)
{
    if (!bytes)
        return std::optional<String>{};

    auto text = String::fromBytes(detail::byteView(*bytes));
    if (!text)
        return Unex(std::move(text).error());
    return std::optional<String>{std::move(*text)};
}

template <std::ranges::input_range Range>
[[nodiscard]] std::vector<std::string> toBytesVector(const Range &texts)
{
    std::vector<std::string> out;
    if constexpr (std::ranges::sized_range<const Range>)
        out.reserve(std::ranges::size(texts));

    for (const auto &text : texts)
        out.push_back(toBytes(text));
    return out;
}

template <std::ranges::input_range Range>
[[nodiscard]] std::vector<std::pair<std::string, std::string>> toBytesPairs(const Range &pairs)
{
    std::vector<std::pair<std::string, std::string>> out;
    if constexpr (std::ranges::sized_range<const Range>)
        out.reserve(std::ranges::size(pairs));

    for (const auto &[first, second] : pairs)
        out.emplace_back(toBytes(first), toBytes(second));
    return out;
}

[[nodiscard]] constexpr String operator+(String lhs, const String &rhs)
{
    lhs += rhs;
    return lhs;
}

template <typename... Ts> String format(std::format_string<Ts...> formatStr, Ts &&...args)
{
    return String::fromBytes(std::format(formatStr, std::forward<Ts>(args)...)).expect();
}

namespace literals {
[[nodiscard]] constexpr String operator""_t(const char *bytes, size_t n)
{
    return String::fromBytes(std::string_view{bytes, n}).expect();
}
} // namespace literals

} // namespace text

using text::String;

template <> struct std::formatter<text::String, char> : std::formatter<std::string_view, char> {
    auto format(const text::String &text, std::format_context &ctx) const
    {
        return std::formatter<std::string_view, char>::format(text.view(), ctx);
    }
};

template <> struct std::hash<String> {
    size_t operator()(const String &text) const noexcept
    {
        return std::hash<std::string_view>{}(text.view());
    }
};
