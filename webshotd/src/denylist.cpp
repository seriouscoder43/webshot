#include "denylist.hpp"
/**
 * @file
 * @brief Host denylist checks and persistence backed by Postgres.
 */
#include <webshot/sql_queries.hpp>

#include "database.hpp"
#include "prefix_utils.hpp"
#include "text_postgres_formatter.hpp"
#include "userver_namespaces.hpp"

#include <format>
#include <string>
#include <utility>

#include <userver/components/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/yaml_config.hpp>
namespace pg = us::storages::postgres;
namespace sql = webshot::sql;

namespace v1 {

us::yaml_config::Schema Denylist::GetStaticConfigSchema()
{
    return us::yaml_config::MergeSchemas<us::components::ComponentBase>(R"(
type: object
description: 'denylist component'
additionalProperties: false
properties: {}
)");
}

struct Denylist::Impl {
    explicit Impl(
        const us::components::ComponentConfig &, const us::components::ComponentContext &context
    )
        : cluster(context.FindComponent<us::components::Postgres>("shared_state_db").GetCluster())
    {
    }

    template <pg::ClusterHostType Host, typename F, typename... Ts>
    [[nodiscard]] auto execDb(F &&f, Ts &&...args) const
    {
        return pgx::execute<Host>(cluster, std::forward<F>(f), std::forward<Ts>(args)...);
    }

    template <typename F, typename... Ts> [[nodiscard]] auto readonly(F &&f, Ts &&...args) const
    {
        return execDb<pg::ClusterHostType::kSlaveOrMaster>(
            std::forward<F>(f), std::forward<Ts>(args)...
        );
    }

    template <typename F, typename... Ts> [[nodiscard]] auto readwrite(F &&f, Ts &&...args) const
    {
        return execDb<pg::ClusterHostType::kMaster>(std::forward<F>(f), std::forward<Ts>(args)...);
    }

    pg::ClusterPtr cluster;
};

Denylist::Denylist(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : us::components::ComponentBase(config, context), impl(std::make_unique<Impl>(config, context))
{
}

Denylist::~Denylist() = default;

Expected<bool, DenylistError> Denylist::isAllowedPrefix(const String &prefixKey)
{
    const auto tree = prefix::makePrefixTree(prefixKey);
    const auto blocked = impl->readonly(
        [&](auto &res) { return res.template AsSingleRow<bool>(); }, sql::kCheckDenylistTree, tree
    );
    if (!blocked) {
        LOG_ERROR() << std::format("denylist check failed: {}", blocked.error().what);
        return Unex(DenylistError::kDbFailure);
    }
    return !*blocked;
}

Expected<void, DenylistError> Denylist::insertPrefix(const String &prefixKey, const String &reason)
{
    const auto tree = prefix::makePrefixTree(prefixKey);
    const auto inserted = impl->readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kInsertDenylistHost, prefixKey, tree,
        reason
    );
    if (!inserted) {
        LOG_CRITICAL() << std::format(
            "denylist insert failed for {}: {}", prefixKey, inserted.error().what
        );
        us::utils::AbortWithStacktrace("denylist insert failed");
    }
    return {};
}

} // namespace v1
