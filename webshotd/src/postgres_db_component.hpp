#pragma once

#include "database.hpp"

#include <utility>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/component.hpp>

namespace ws {

namespace us = userver;
namespace pg = us::storages::postgres;

/**
 * @brief Common base for typed Postgres components.
 *
 * Provides RunReadonly/RunReadwrite helpers so call sites never mention vague
 * master or slave host types directly.
 */
class [[nodiscard]] PostgresDbComponent : public us::components::Postgres {
public:
    explicit PostgresDbComponent(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    )
        : us::components::Postgres(config, context)
    {
    }

    template <typename F, typename... Ts> [[nodiscard]] auto RunReadonly(F &&f, Ts &&...args) const
    {
        return pgx::Execute<pg::ClusterHostType::kSlaveOrMaster>(
            GetCluster(), std::forward<F>(f), std::forward<Ts>(args)...
        );
    }

    template <typename F, typename... Ts> [[nodiscard]] auto RunReadwrite(F &&f, Ts &&...args) const
    {
        return pgx::Execute<pg::ClusterHostType::kMaster>(
            GetCluster(), std::forward<F>(f), std::forward<Ts>(args)...
        );
    }

    template <typename F> [[nodiscard]] auto RunReadwriteTransaction(F &&f) const
    {
        return pgx::ReadwriteTransaction(GetCluster(), std::forward<F>(f));
    }
};

} // namespace ws
