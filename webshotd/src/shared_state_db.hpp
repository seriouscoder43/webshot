#pragma once

#include "postgres_db_component.hpp"

namespace ws {

class [[nodiscard]] SharedStateDb final : public PostgresDbComponent {
public:
    using Base = PostgresDbComponent;
    static constexpr std::string_view kName = "shared_state_db";

    explicit SharedStateDb(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    )
        : Base(config, context)
    {
    }
};

} // namespace ws
