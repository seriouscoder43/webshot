#pragma once

#include "expected.hpp"
#include "invariant.hpp"
#include "try.hpp"

#include "chrono.hpp"
#include <algorithm>

#include <userver/engine/deadline.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/task_inherited_data.hpp>

namespace ws {
namespace us = userver;
namespace server = us::server;
namespace eng = us::engine;
using text::literals::operator""_t;
using namespace ws::chrono_literals;

enum class DeadlineError {
    kTimeout,
};

template <typename Rep, typename Period>
[[nodiscard]] inline eng::Deadline DeadlineAfter(const std::chrono::duration<Rep, Period> &delay)
{
    return eng::Deadline::FromDuration(delay);
}

template <typename Clock, typename Duration>
    requires requires { typename Clock::base::time_point; }
[[nodiscard]] inline eng::Deadline DeadlineAt(const std::chrono::time_point<Clock, Duration> &when)
{
    return eng::Deadline::FromTimePoint(
        ws::chrono::ToStdTimePoint<typename Clock::base::time_point>(when)
    );
}

[[nodiscard]] inline ws::chrono::nanoseconds TimeLeftOrZero(eng::Deadline deadline) noexcept
{
    using namespace ws::chrono_literals;
    using Ns = ws::chrono::nanoseconds;

    if (!deadline.IsReachable())
        return Ns::max();

    auto left = deadline.TimeLeft();
    if (left <= decltype(left)::zero())
        return 0_ns;

    auto left_ns = ws::chrono::DurationCast<Ns>(left);
    if (left_ns <= 0_ns)
        return 0_ns;
    return left_ns;
}

[[nodiscard]] inline eng::Deadline PickEarlierDeadline(eng::Deadline a, eng::Deadline b)
{
    const bool a_reachable = a.IsReachable();
    const bool b_reachable = b.IsReachable();

    if (a_reachable && !b_reachable)
        return a;
    if (!a_reachable && b_reachable)
        return b;
    if (!a_reachable && !b_reachable)
        return a;

    auto a_left = TimeLeftOrZero(a);
    auto b_left = TimeLeftOrZero(b);
    if (a_left <= b_left)
        return a;
    return b;
}

template <typename Rep, typename Period>
[[nodiscard]] inline eng::Deadline
ClampDeadline(eng::Deadline deadline, const std::chrono::duration<Rep, Period> &max_wait)
{
    return PickEarlierDeadline(deadline, DeadlineAfter(max_wait));
}

[[nodiscard]] inline eng::Deadline ComputeHandlerDeadline(
    const server::http::HttpRequest &request, ws::chrono::milliseconds handler_timeout
)
{
    auto request_start = ws::chrono::FromStdSteadyTimePoint(request.GetStartTime());
    auto config_deadline = DeadlineAt(request_start + handler_timeout);
    auto inherited_deadline = server::request::GetTaskInheritedDeadline();

    return PickEarlierDeadline(config_deadline, inherited_deadline);
}

[[nodiscard]] inline ws::chrono::milliseconds TimeLeftOrZeroMs(eng::Deadline deadline) noexcept
{
    using namespace ws::chrono_literals;
    using Ms = ws::chrono::milliseconds;

    if (!deadline.IsReachable())
        return Ms::max();

    auto left_ms = ws::chrono::DurationCast<Ms>(TimeLeftOrZero(deadline));
    if (left_ms <= 0_ms)
        return 0_ms;
    return left_ms;
}

[[nodiscard]] inline Expected<ws::chrono::milliseconds, DeadlineError>
TimeLeftMs(eng::Deadline deadline) noexcept
{
    if (deadline.IsReachable() && deadline.IsReached())
        return Unex(DeadlineError::kTimeout);
    return TimeLeftOrZeroMs(deadline);
}

[[nodiscard]] inline Expected<void, DeadlineError>
SleepWithinDeadline(eng::Deadline deadline, ws::chrono::milliseconds delay)
{
    using namespace ws::chrono_literals;

    if (delay <= 0_ms)
        return {};

    auto sleep_for = std::min(delay, TRY(TimeLeftMs(deadline)));
    eng::SleepFor(sleep_for);
    if (sleep_for != delay)
        return Unex(DeadlineError::kTimeout);

    return {};
}

[[nodiscard]] inline Expected<void, DeadlineError> SleepUntilDeadline(eng::Deadline deadline)
{
    using namespace ws::chrono_literals;

    Invariant(deadline.IsReachable(), "sleepUntilDeadline requires a reachable deadline"_t);

    auto remaining = TRY(TimeLeftMs(deadline));
    ENSURE(remaining > 0_ms, DeadlineError::kTimeout);

    eng::SleepFor(remaining);
    return {};
}

} // namespace ws
