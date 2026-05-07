#include "access_policy.hpp"
/**
 * @file
 * @brief Link prefix access policy checks and persistence backed by Postgres.
 */
#include <webshot/sql_queries.hpp>

#include "database.hpp"
#include "prefix_utils.hpp"
#include "text_postgres_formatter.hpp"
#include "try.hpp"

#include <format>
#include <string>
#include <utility>

#include <userver/components/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/yaml_config.hpp>
namespace ws {
namespace us = userver;
namespace pg = us::storages::postgres;
using namespace text::literals;
namespace sql = webshot::sql;

String AccessDecisionMessage(AccessDecisionReason reason)
{
    using enum AccessDecisionReason;

    switch (reason) {
    case kAllowed:
        return "allowed"_t;
    case kDenylisted:
        return "link prefix in denylist"_t;
    case kNotAllowlisted:
        return "link not in allowlist"_t;
    case kNonHttps:
        return "non-HTTPS fetch blocked"_t;
    }
}

us::yaml_config::Schema AccessPolicyStore::GetStaticConfigSchema()
{
    return us::yaml_config::MergeSchemas<us::components::ComponentBase>(R"(
type: object
description: 'access policy store component'
additionalProperties: false
properties: {}
)");
}

struct AccessPolicyStore::Impl {
    explicit Impl(
        const us::components::ComponentConfig &, const us::components::ComponentContext &context
    )
        : cluster(context.FindComponent<us::components::Postgres>("shared_state_db").GetCluster())
    {
    }

    template <pg::ClusterHostType Host, typename F, typename... Ts>
    [[nodiscard]] auto ExecDb(F &&f, Ts &&...args) const
    {
        return pgx::Execute<Host>(cluster, std::forward<F>(f), std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts> [[nodiscard]] auto Readonly(F &&f, Ts &&...args) const
    {
        return ExecDb<pg::ClusterHostType::kSlaveOrMaster>(
            std::forward<F>(f), std::forward<Ts>(args)...
        );
    }

    template <typename F, typename... Ts> [[nodiscard]] auto Readwrite(F &&f, Ts &&...args) const
    {
        return ExecDb<pg::ClusterHostType::kMaster>(std::forward<F>(f), std::forward<Ts>(args)...);
    }

    pg::ClusterPtr cluster;
};

AccessPolicyStore::AccessPolicyStore(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : us::components::ComponentBase(config, context), impl_(std::make_unique<Impl>(config, context))
{
}

AccessPolicyStore::~AccessPolicyStore() = default;

Expected<bool, AccessPolicyError> AccessPolicyStore::IsAllowedPrefix(const String &prefix_key)
{
    return !TRY(IsDeniedPrefix(prefix_key));
}

Expected<bool, AccessPolicyError> AccessPolicyStore::IsDeniedPrefix(const String &prefix_key)
{
    const auto tree = prefix::MakePrefixTree(prefix_key);
    auto denied = impl_->Readonly(
        [&](auto &res) { return res.template AsSingleRow<bool>(); }, sql::kCheckDenylistTree, tree
    );
    if (!denied)
        LOG_ERROR() << std::format("access policy check failed: {}", denied.Error().what);
    return TRY_ERR_AS(std::move(denied), AccessPolicyError::kDbError);
}

Expected<bool, AccessPolicyError> AccessPolicyStore::IsAllowlistedPrefix(const String &prefix_key)
{
    const auto tree = prefix::MakePrefixTree(prefix_key);
    auto allowlisted = impl_->Readonly(
        [&](auto &res) { return res.template AsSingleRow<bool>(); }, sql::kCheckAllowlistTree, tree
    );
    if (!allowlisted)
        LOG_ERROR() << std::format("allowlist check failed: {}", allowlisted.Error().what);
    return TRY_ERR_AS(std::move(allowlisted), AccessPolicyError::kDbError);
}

Expected<AccessDecision, AccessPolicyError>
AccessPolicyStore::EvaluatePrefix(const String &prefix_key, AccessPolicyMode mode)
{
    using enum AccessDecisionReason;
    using enum AccessPolicyMode;

    if (mode == kRegular) {
        const auto allowlisted = TRY(IsAllowlistedPrefix(prefix_key));
        if (allowlisted)
            return AccessDecision{.allowed = true, .reason = kAllowed};

        const auto denied = TRY(IsDeniedPrefix(prefix_key));
        if (denied)
            return AccessDecision{.allowed = false, .reason = kDenylisted};
        return AccessDecision{.allowed = true, .reason = kAllowed};
    }

    const auto denied = TRY(IsDeniedPrefix(prefix_key));
    if (denied)
        return AccessDecision{.allowed = false, .reason = kDenylisted};

    const auto allowlisted = TRY(IsAllowlistedPrefix(prefix_key));
    if (!allowlisted)
        return AccessDecision{.allowed = false, .reason = kNotAllowlisted};
    return AccessDecision{.allowed = true, .reason = kAllowed};
}

Expected<void, AccessPolicyError>
AccessPolicyStore::InsertPrefix(const String &prefix_key, const String &reason)
{
    const auto tree = prefix::MakePrefixTree(prefix_key);
    const auto inserted = impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kInsertDenylistPrefix, prefix_key, tree,
        reason
    );
    if (!inserted) {
        LOG_CRITICAL() << std::format(
            "denylist insert failed for {}: {}", prefix_key, inserted.Error().what
        );
        us::utils::AbortWithStacktrace("denylist insert failed");
    }
    return {};
}

Expected<void, AccessPolicyError>
AccessPolicyStore::InsertAllowlistPrefix(const String &prefix_key, const String &reason)
{
    const auto tree = prefix::MakePrefixTree(prefix_key);
    const auto inserted = impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kInsertAllowlistPrefix, prefix_key, tree,
        reason
    );
    if (!inserted) {
        LOG_ERROR() << std::format(
            "allowlist insert failed for {}: {}", prefix_key, inserted.Error().what
        );
        return Unex(AccessPolicyError::kDbError);
    }
    return {};
}

Expected<void, AccessPolicyError> AccessPolicyStore::RemoveAllowlistPrefix(const String &prefix_key)
{
    const auto removed = impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kDeleteAllowlistPrefix, prefix_key
    );
    if (!removed) {
        LOG_ERROR() << std::format(
            "allowlist remove failed for {}: {}", prefix_key, removed.Error().what
        );
        return Unex(AccessPolicyError::kDbError);
    }
    return {};
}

} // namespace ws
