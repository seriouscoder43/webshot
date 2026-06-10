#pragma once

#include "integers.hpp"

#include <chrono>
#include <concepts>
#include <ctime>
#include <type_traits>
#include <utility>

#include <userver/utils/datetime.hpp>
#include <userver/utils/datetime/timepoint_tz.hpp>

namespace std {

template <> struct common_type<i64, i64> {
    using type = i64;
};

template <typename T>
    requires integral<remove_cvref_t<T>> && is_signed_v<remove_cvref_t<T>> &&
             (!same_as<remove_cvref_t<T>, bool>) && (!same_as<remove_cvref_t<T>, i64>)
struct common_type<i64, T> {
    using type = i64;
};

template <typename T>
    requires integral<remove_cvref_t<T>> && is_signed_v<remove_cvref_t<T>> &&
             (!same_as<remove_cvref_t<T>, bool>) && (!same_as<remove_cvref_t<T>, i64>)
struct common_type<T, i64> {
    using type = i64;
};

} // namespace std

namespace ws::chrono {
namespace us = userver;
namespace datetime = us::utils::datetime;

template <typename Period> using duration = std::chrono::duration<i64, Period>;

using nanoseconds = duration<std::nano>;
using microseconds = duration<std::micro>;
using milliseconds = duration<std::milli>;
using seconds = duration<std::ratio<1>>;
using minutes = duration<std::ratio<60>>;
using hours = duration<std::ratio<3600>>;

template <typename Clock, typename Duration = typename Clock::duration>
using time_point = std::chrono::time_point<Clock, Duration>;

template <typename ToDuration, typename Rep, typename Period>
[[nodiscard]] constexpr ToDuration
DurationCast(const std::chrono::duration<Rep, Period> &value) noexcept
{
    return std::chrono::duration_cast<ToDuration>(value);
}

template <typename ToDuration, typename Rep, typename Period>
[[nodiscard]] constexpr ToDuration
FromStdDuration(const std::chrono::duration<Rep, Period> &value) noexcept
{
    return DurationCast<ToDuration>(value);
}

template <typename ToDuration, typename Rep, typename Period>
[[nodiscard]] constexpr ToDuration
ToStdDuration(const std::chrono::duration<Rep, Period> &value) noexcept
{
    return std::chrono::duration_cast<ToDuration>(value);
}

template <typename ToTimePoint, typename Clock, typename Duration>
[[nodiscard]] constexpr ToTimePoint
FromStdTimePoint(const std::chrono::time_point<Clock, Duration> &value) noexcept
{
    return ToTimePoint{DurationCast<typename ToTimePoint::duration>(value.time_since_epoch())};
}

template <typename ToTimePoint, typename Clock, typename Duration>
[[nodiscard]] constexpr ToTimePoint
ToStdTimePoint(const std::chrono::time_point<Clock, Duration> &value) noexcept
{
    return ToTimePoint{
        std::chrono::duration_cast<typename ToTimePoint::duration>(value.time_since_epoch())
    };
}

struct SystemClock {
    using base = std::chrono::system_clock;
    using rep = i64;
    using period = typename base::period;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<SystemClock, duration>;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr bool is_steady = base::is_steady;

    [[nodiscard]] static time_point Now() noexcept
    {
        return FromStdTimePoint<time_point>(datetime::Now());
    }

    [[nodiscard]] static std::time_t ToTimeT(const time_point &tp) noexcept
    {
        auto std_tp = ToStdTimePoint<base::time_point>(tp);
        return base::to_time_t(std_tp);
    }

    [[nodiscard]] static std::time_t ToTimeT(const base::time_point &tp) noexcept
    {
        return base::to_time_t(tp);
    }

    [[nodiscard]] static time_point FromTimeT(std::time_t t) noexcept
    {
        return FromStdTimePoint<time_point>(base::from_time_t(t));
    }
};

struct SteadyClock {
    using base = std::chrono::steady_clock;
    using rep = i64;
    using period = typename base::period;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<SteadyClock, duration>;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr bool is_steady = base::is_steady;

    [[nodiscard]] static time_point Now() noexcept
    {
        return FromStdTimePoint<time_point>(datetime::SteadyNow());
    }
};

[[nodiscard]] inline SystemClock::time_point
FromStdSystemTimePoint(const SystemClock::base::time_point &value) noexcept
{
    return FromStdTimePoint<SystemClock::time_point>(value);
}

[[nodiscard]] inline SystemClock::base::time_point
ToStdSystemTimePoint(const SystemClock::time_point &value) noexcept
{
    return ToStdTimePoint<SystemClock::base::time_point>(value);
}

[[nodiscard]] inline SteadyClock::time_point
FromStdSteadyTimePoint(const SteadyClock::base::time_point &value) noexcept
{
    return FromStdTimePoint<SteadyClock::time_point>(value);
}

[[nodiscard]] inline SteadyClock::base::time_point
ToStdSteadyTimePoint(const SteadyClock::time_point &value) noexcept
{
    return ToStdTimePoint<SteadyClock::base::time_point>(value);
}

template <typename Format>
[[nodiscard]] inline auto UtcTimestring(SystemClock::time_point value, Format &&format)
{
    return datetime::UtcTimestring(ToStdSystemTimePoint(value), std::forward<Format>(format));
}

[[nodiscard]] inline datetime::TimePointTz ToTimePointTz(SystemClock::time_point value)
{
    return datetime::TimePointTz(ToStdSystemTimePoint(value));
}

[[nodiscard]] inline SystemClock::time_point Now() noexcept { return SystemClock::Now(); }

[[nodiscard]] inline SteadyClock::time_point SteadyNow() noexcept { return SteadyClock::Now(); }

} // namespace ws::chrono

namespace ws::chrono_literals {

[[nodiscard]] constexpr chrono::nanoseconds operator""_ns(unsigned long long value) noexcept
{
    return chrono::nanoseconds{i64{value}};
}

[[nodiscard]] constexpr chrono::microseconds operator""_us(unsigned long long value) noexcept
{
    return chrono::microseconds{i64{value}};
}

[[nodiscard]] constexpr chrono::milliseconds operator""_ms(unsigned long long value) noexcept
{
    return chrono::milliseconds{i64{value}};
}

[[nodiscard]] constexpr chrono::seconds operator""_s(unsigned long long value) noexcept
{
    return chrono::seconds{i64{value}};
}

[[nodiscard]] constexpr chrono::minutes operator""_min(unsigned long long value) noexcept
{
    return chrono::minutes{i64{value}};
}

[[nodiscard]] constexpr chrono::hours operator""_h(unsigned long long value) noexcept
{
    return chrono::hours{i64{value}};
}

} // namespace ws::chrono_literals
