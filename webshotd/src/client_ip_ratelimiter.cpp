#include "client_ip_ratelimiter.hpp"
/**
 * @file
 * @brief In-process per-client-IP rate limiter.
 */

#include "invariant.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

#include <userver/cache/expirable_lru_cache.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/token_bucket.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace ws {

namespace us = userver;
namespace concurrent = us::concurrent;
namespace eng = us::engine;
namespace datetime = us::utils::datetime;
namespace chrono = std::chrono;

using namespace std::chrono_literals;
using namespace text::literals;

using us::utils::TokenBucket;

namespace {

struct [[nodiscard]] ClientIpLimiterState final {
    TokenBucket bucket;
    TokenBucket::TimePoint last_allow;
};

} // namespace

struct ClientIpLimiter final {
    concurrent::Variable<ClientIpLimiterState, eng::Mutex> state;

    explicit ClientIpLimiter(ClientIpLimiterState state) : state(std::move(state)) {}

    [[nodiscard]] auto UniqueLock() { return state.UniqueLock(); }
};

ClientIpRatelimiter::ClientIpRatelimiter(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : Base(config, context), interval(config["interval_ms"].As<uint64_t>() * 1ms)
{
}

ClientIpRatelimiter::~ClientIpRatelimiter() = default;

us::yaml_config::Schema ClientIpRatelimiter::GetStaticConfigSchema()
{
    return us::yaml_config::MergeSchemas<Base>(R"(
type: object
description: In-process per-client-IP rate limiter
additionalProperties: false
properties:
  interval_ms:
    type: integer
    minimum: 0
    description: Per-client-IP minimum interval between some HTTP operations in milliseconds; 0 disables ratelimit
)");
}

std::shared_ptr<ClientIpLimiter> ClientIpRatelimiter::DoGetByKey(const Ip &)
{
    return std::make_shared<ClientIpLimiter>(ClientIpLimiterState{
        .bucket =
            TokenBucket{
                1,
                TokenBucket::RefillPolicy{
                    .amount = 1, .interval = chrono::duration_cast<TokenBucket::Duration>(interval)
                }
            },
        .last_allow = datetime::SteadyNow(),
    });
}

std::optional<ClientIpRatelimit> ClientIpRatelimiter::Acquire(const Ip &client_ip) noexcept
{
    if (interval == 0ms)
        return {};

    auto cache = GetCache();
    std::shared_ptr<ClientIpLimiter> limiter = cache.Get(client_ip);
    Invariant(limiter, "failed to allocate per-IP ratelimit entry"_t);

    auto now_tp = datetime::SteadyNow();
    auto lock = limiter->UniqueLock();

    if (lock->bucket.Obtain()) {
        lock->last_allow = now_tp;
        return {};
    }
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(now_tp - lock->last_allow);
    auto retry_after = 0ms;
    if (elapsed < interval)
        retry_after = interval - elapsed;
    return ClientIpRatelimit{.retry_after = retry_after};
}

} // namespace ws
