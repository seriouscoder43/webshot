#include "chrono.hpp"

#include <userver/utest/utest.hpp>
#include <userver/utils/datetime.hpp>

#include "s3/refresh_utils.hpp"

namespace ws {
namespace us = userver;
} // namespace ws

using namespace ws;

using ws::s3refresh::ComputeRefreshDelay;
using namespace ws::chrono_literals;

UTEST(RefreshSchedule, FutureExpirationRespectsMargin)
{
    auto now = ws::chrono::Now();
    auto expires_at = now + 600_s;
    auto delay = ComputeRefreshDelay(now, expires_at, 120_s);
    EXPECT_EQ(delay, 480_s);
}

UTEST(RefreshSchedule, PastOrNearExpirationClampsToZero)
{
    auto now = ws::chrono::Now();
    auto expires_at = now + 30_s;
    auto delay = ComputeRefreshDelay(now, expires_at, 60_s);
    EXPECT_EQ(delay, 0_s);
}
