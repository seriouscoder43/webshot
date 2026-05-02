#include <chrono>

#include <userver/utest/utest.hpp>
#include <userver/utils/datetime.hpp>

#include "s3_refresh_utils.hpp"

namespace v1 {
namespace us = userver;
namespace datetime = us::utils::datetime;
} // namespace v1

using namespace v1;

using std::chrono::system_clock;
using v1::s3refresh::ComputeRefreshDelay;
using namespace std::chrono_literals;

UTEST(S3RefreshSchedule, FutureExpirationRespectsMargin)
{
    const auto now = datetime::Now();
    const auto expires_at = now + 600s;
    const auto delay = ComputeRefreshDelay(now, expires_at, 120s);
    EXPECT_EQ(delay, 480s);
}

UTEST(S3RefreshSchedule, PastOrNearExpirationClampsToZero)
{
    const auto now = datetime::Now();
    const auto expires_at = now + 30s;
    const auto delay = ComputeRefreshDelay(now, expires_at, 60s);
    EXPECT_EQ(delay, 0s);
}
