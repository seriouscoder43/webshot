#pragma once

#include "userver_namespaces.hpp"

#include <boost/safe_numerics/checked_default.hpp>
#include <boost/safe_numerics/checked_result_operations.hpp>
#include <boost/safe_numerics/exception_policies.hpp>
#include <boost/safe_numerics/safe_base_operations.hpp>
#include <boost/safe_numerics/safe_common.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

#include <userver/utils/assert.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <limits>
#include <string_view>
#include <type_traits>

namespace integers {

struct SafeIntegerAbort {
    [[noreturn]] void
    operator()(const boost::safe_numerics::safe_numerics_error &e, const char *msg) const noexcept
    {
        const char *msgSafe = msg ? msg : "(no details)";
        const char *errorSafe = boost::safe_numerics::literal_string(e);

        std::array<char, 512> buf{};
        const int written = std::snprintf(
            buf.data(), buf.size(), "safe integer failure: %s: %s",
            errorSafe ? errorSafe : "(unknown)", msgSafe
        );

        if (written <= 0) {
            us::utils::AbortWithStacktrace("safe integer operation failed");
        }

        const size_t len = static_cast<size_t>(written) < buf.size() ? static_cast<size_t>(written)
                                                                     : (buf.size() - 1);
        us::utils::AbortWithStacktrace(std::string_view{buf.data(), len});
    }
};

struct TrapUninitialized {
    template <typename... Args> constexpr void operator()(Args &&...) const
    {
        static_assert(sizeof...(Args) == 0, "safe integers must be explicitly initialized");
    }
};

using AbortPolicy = boost::safe_numerics::exception_policy<
    SafeIntegerAbort, SafeIntegerAbort, SafeIntegerAbort, TrapUninitialized>;

template <typename T>
using SafeInteger = boost::safe_numerics::safe<T, boost::safe_numerics::native, AbortPolicy>;

using u16 = SafeInteger<uint16_t>;
using u32 = SafeInteger<uint32_t>;
using i32 = SafeInteger<int32_t>;
using u64 = SafeInteger<uint64_t>;
using i64 = SafeInteger<int64_t>;
using usize = SafeInteger<size_t>;

static_assert(std::numeric_limits<u16>::is_specialized);
static_assert(std::numeric_limits<u32>::is_specialized);
static_assert(std::numeric_limits<i32>::is_specialized);
static_assert(std::numeric_limits<u64>::is_specialized);
static_assert(std::numeric_limits<i64>::is_specialized);
static_assert(std::numeric_limits<usize>::is_specialized);

static_assert(std::is_same_v<decltype(std::numeric_limits<u16>::max()), u16>);
static_assert(std::is_same_v<decltype(std::numeric_limits<u32>::max()), u32>);
static_assert(std::is_same_v<decltype(std::numeric_limits<i32>::max()), i32>);
static_assert(std::is_same_v<decltype(std::numeric_limits<u64>::max()), u64>);
static_assert(std::is_same_v<decltype(std::numeric_limits<i64>::max()), i64>);
static_assert(std::is_same_v<decltype(std::numeric_limits<usize>::max()), usize>);

template <typename T>
concept NumericCastType = std::integral<std::remove_cvref_t<T>> ||
                          std::is_enum_v<std::remove_cvref_t<T>>;

template <typename To, typename From>
    requires NumericCastType<To> && NumericCastType<From>
[[nodiscard]] constexpr std::remove_cvref_t<To> numericCast(From value) noexcept
{
    using ToValue = std::remove_cvref_t<To>;
    using FromValue = std::remove_cvref_t<From>;

    if constexpr (std::is_enum_v<ToValue>) {
        using ToUnderlying = std::underlying_type_t<ToValue>;
        return static_cast<ToValue>(numericCast<ToUnderlying>(value));
    } else if constexpr (std::is_enum_v<FromValue>) {
        using FromUnderlying = std::underlying_type_t<FromValue>;
        return numericCast<ToValue>(static_cast<FromUnderlying>(value));
    } else {
        return static_cast<ToValue>(SafeInteger<ToValue>{SafeInteger<FromValue>{value}});
    }
}

template <typename T, typename PromotionPolicy, typename ExceptionPolicy>
[[nodiscard]] constexpr T
numericCast(const boost::safe_numerics::safe<T, PromotionPolicy, ExceptionPolicy> &value) noexcept
{
    return static_cast<T>(value);
}

template <typename To, typename T, typename PromotionPolicy, typename ExceptionPolicy>
[[nodiscard]] constexpr std::remove_cvref_t<To>
numericCast(const boost::safe_numerics::safe<T, PromotionPolicy, ExceptionPolicy> &value) noexcept
    requires NumericCastType<To> && (!std::same_as<std::remove_cvref_t<To>, T>)
{
    using ToValue = std::remove_cvref_t<To>;

    if constexpr (std::is_enum_v<ToValue>) {
        using ToUnderlying = std::underlying_type_t<ToValue>;
        return static_cast<ToValue>(numericCast<ToUnderlying>(numericCast(value)));
    } else {
        return static_cast<ToValue>(SafeInteger<ToValue>{numericCast(value)});
    }
}

template <typename T, typename PromotionPolicy, typename ExceptionPolicy>
[[nodiscard]] constexpr T
raw(const boost::safe_numerics::safe<T, PromotionPolicy, ExceptionPolicy> &value) noexcept
{
    return numericCast(value);
}

struct SSizeFn {
    template <typename C> [[nodiscard]] constexpr i64 operator()(const C &c) const noexcept
    {
        const auto sizeValue = usize(c.size());
        return i64(sizeValue);
    }
};

struct USizeFn {
    template <typename C>
        requires requires(const C &c) { c.size(); }
    [[nodiscard]] constexpr usize operator()(const C &c) const noexcept
    {
        return usize(c.size());
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr SSizeFn ssize{};
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr USizeFn usz{};

} // namespace integers

namespace integers::literals {

[[nodiscard]] constexpr u16 operator""_u16(unsigned long long value) noexcept { return u16(value); }

[[nodiscard]] constexpr u32 operator""_u32(unsigned long long value) noexcept { return u32(value); }

[[nodiscard]] constexpr i32 operator""_i32(unsigned long long value) noexcept { return i32(value); }

[[nodiscard]] constexpr u64 operator""_u64(unsigned long long value) noexcept { return u64(value); }

[[nodiscard]] constexpr i64 operator""_i64(unsigned long long value) noexcept { return i64(value); }

[[nodiscard]] constexpr usize operator""_uz(unsigned long long value) noexcept
{
    return usize(value);
}

} // namespace integers::literals

using integers::i32;
using integers::i64;
using integers::numericCast;
using integers::raw;
using integers::SafeInteger;
using integers::ssize;
using integers::u16;
using integers::u32;
using integers::u64;
using integers::usize;
using integers::usz;
using namespace integers::literals;

namespace std {

template <> struct formatter<u16, char> : formatter<uint16_t, char> {
    auto format(const u16 &value, format_context &ctx) const
    {
        return formatter<uint16_t, char>::format(integers::raw(value), ctx);
    }
};

template <> struct formatter<u32, char> : formatter<uint32_t, char> {
    auto format(const u32 &value, format_context &ctx) const
    {
        return formatter<uint32_t, char>::format(integers::raw(value), ctx);
    }
};

template <> struct formatter<i32, char> : formatter<int32_t, char> {
    auto format(const i32 &value, format_context &ctx) const
    {
        return formatter<int32_t, char>::format(integers::raw(value), ctx);
    }
};

template <> struct formatter<u64, char> : formatter<uint64_t, char> {
    auto format(const u64 &value, format_context &ctx) const
    {
        return formatter<uint64_t, char>::format(integers::raw(value), ctx);
    }
};

template <> struct formatter<i64, char> : formatter<int64_t, char> {
    auto format(const i64 &value, format_context &ctx) const
    {
        return formatter<int64_t, char>::format(integers::raw(value), ctx);
    }
};

template <> struct hash<u16> {
    size_t operator()(const u16 &value) const noexcept
    {
        return hash<uint16_t>{}(integers::raw(value));
    }
};

template <> struct hash<u32> {
    size_t operator()(const u32 &value) const noexcept
    {
        return hash<uint32_t>{}(integers::raw(value));
    }
};

template <> struct hash<i32> {
    size_t operator()(const i32 &value) const noexcept
    {
        return hash<int32_t>{}(integers::raw(value));
    }
};

template <> struct hash<u64> {
    size_t operator()(const u64 &value) const noexcept
    {
        return hash<uint64_t>{}(integers::raw(value));
    }
};

template <> struct hash<i64> {
    size_t operator()(const i64 &value) const noexcept
    {
        return hash<int64_t>{}(integers::raw(value));
    }
};

} // namespace std
