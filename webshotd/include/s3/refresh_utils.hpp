#pragma once

#include "chrono.hpp"

namespace ws::s3refresh {

/**
 * @brief Compute delay before refreshing STS credentials.
 *
 * Returns max(expiresAt - now - margin, 0_s) in whole seconds.
 */
[[nodiscard]] inline ws::chrono::seconds ComputeRefreshDelay(
    ws::chrono::SystemClock::time_point now, ws::chrono::SystemClock::time_point expires_at,
    ws::chrono::seconds margin
)
{
    using namespace ws::chrono_literals;

    auto delay = expires_at - now - margin;
    if (delay < 0_s)
        return 0_s;
    return ws::chrono::DurationCast<ws::chrono::seconds>(delay);
}

} // namespace ws::s3refresh
