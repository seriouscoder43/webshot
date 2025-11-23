#pragma once

#include <chrono>

#include <userver/engine/deadline.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/task_inherited_data.hpp>

namespace v1 {

[[nodiscard]] inline userver::engine::Deadline
pickEarlierDeadline(userver::engine::Deadline a, userver::engine::Deadline b)
{
    const bool aReachable = a.IsReachable();
    const bool bReachable = b.IsReachable();

    if (aReachable && !bReachable)
        return a;
    if (!aReachable && bReachable)
        return b;
    if (!aReachable && !bReachable)
        return a;

    if (a.TimeLeft() <= b.TimeLeft())
        return a;
    return b;
}

[[nodiscard]] inline userver::engine::Deadline computeHandlerDeadline(
    const userver::server::http::HttpRequest &request, std::chrono::milliseconds handlerTimeout
)
{
    using userver::engine::Deadline;

    const auto configDeadline = Deadline::FromTimePoint(request.GetStartTime() + handlerTimeout);
    const auto inheritedDeadline = userver::server::request::GetTaskInheritedDeadline();

    return pickEarlierDeadline(configDeadline, inheritedDeadline);
}

} // namespace v1
