#pragma once

#include "grab_value.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace v1::detail {

template <typename T> struct IsOptional : std::false_type {};
template <typename T> struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
struct IsTrySupported
    : std::bool_constant<IsExpected<RemoveCvref<T>>::value || IsOptional<RemoveCvref<T>>::value> {};

struct TryEmptyReturn final {
    template <typename T> constexpr operator std::optional<T>() const noexcept { return {}; }
};

template <typename E> struct TryExpectedFailure final {
    template <typename T> constexpr operator std::optional<T>() const noexcept { return {}; }

    template <typename T> [[nodiscard]] operator Expected<T, E>() & { return Unex(error); }

    template <typename T> [[nodiscard]] operator Expected<T, E>() const & { return Unex(error); }

    template <typename T> [[nodiscard]] operator Expected<T, E>() &&
    {
        return Unex(std::move(error));
    }

    E error;
};

template <typename T, typename E>
[[nodiscard]] constexpr bool tryHasValue(const Expected<T, E> &expected) noexcept
{
    return expected.hasValue();
}

template <typename T> [[nodiscard]] constexpr bool tryHasValue(const std::optional<T> &opt) noexcept
{
    return opt.has_value();
}

template <typename T, typename E> [[nodiscard]] inline Unex<E> tryAsUnex(Expected<T, E> &expected)
{
    return Unex(expected.error());
}

template <typename T, typename E>
[[nodiscard]] inline Unex<E> tryAsUnex(const Expected<T, E> &expected)
{
    return Unex(expected.error());
}

template <typename T, typename E> [[nodiscard]] inline Unex<E> tryAsUnex(Expected<T, E> &&expected)
{
    return Unex(std::move(expected).error());
}

template <typename T, typename E> inline T tryExtract(Expected<T, E> &expected)
{
    return grabValueOf(expected);
}

template <typename T, typename E> inline T tryExtract(Expected<T, E> &&expected)
{
    return grabValueOf(std::move(expected));
}

template <typename E> inline void tryExtract(Expected<void, E> &expected) { expected.value(); }

template <typename E> inline void tryExtract(Expected<void, E> &&expected)
{
    std::move(expected).value();
}

template <typename T> inline T tryExtract(std::optional<T> &opt) { return grabValueOf(opt); }

template <typename T> inline T tryExtract(std::optional<T> &&opt)
{
    return grabValueOf(std::move(opt));
}

template <typename T> [[nodiscard]] inline auto tryFailure(T &&value)
{
    static_assert(IsTrySupported<T>::value, "TRY only supports v1::Expected and std::optional");

    if constexpr (IsExpected<RemoveCvref<T>>::value) {
        return TryExpectedFailure{std::forward<T>(value).error()};
    } else {
        return TryEmptyReturn{};
    }
}

} // namespace v1::detail

#if defined(__clang__)
#define V1_TRY_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#define V1_TRY_DIAGNOSTIC_IGNORE                                                                   \
    _Pragma("clang diagnostic ignored \"-Wgnu-statement-expression-from-macro-expansion\"")        \
        _Pragma("clang diagnostic ignored \"-Wshadow\"")
#define V1_TRY_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define V1_TRY_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#define V1_TRY_DIAGNOSTIC_IGNORE                                                                   \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"") _Pragma("GCC diagnostic ignored \"-Wshadow\"")
#define V1_TRY_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#else
#error "TRY requires compiler support for GNU statement expressions"
#endif

#ifdef TRY
#error "TRY is already defined"
#endif

#define TRY(...)                                                                                   \
    V1_TRY_DIAGNOSTIC_PUSH V1_TRY_DIAGNOSTIC_IGNORE({                                              \
        auto &&_temporaryTryResult = (__VA_ARGS__);                                                \
        static_assert(                                                                             \
            ::v1::detail::IsTrySupported<decltype(_temporaryTryResult)>::value,                    \
            "TRY only supports v1::Expected and std::optional"                                     \
        );                                                                                         \
        if (!::v1::detail::tryHasValue(_temporaryTryResult)) [[unlikely]] {                        \
            return ::v1::detail::tryFailure(                                                       \
                std::forward<decltype(_temporaryTryResult)>(_temporaryTryResult)                   \
            );                                                                                     \
        }                                                                                          \
        ::v1::detail::tryExtract(                                                                  \
            std::forward<decltype(_temporaryTryResult)>(_temporaryTryResult)                       \
        );                                                                                         \
    }) V1_TRY_DIAGNOSTIC_POP
