#pragma once
#include <string_view>

namespace sql {

inline constexpr std::string_view kInsertWebshot = R"~(
insert into webshot(id, created_at, url)
values(default, default, $1)
returning id
)~";

inline constexpr std::string_view kSelectWebshot = R"~(
select id from webshot where id = $1 limit $2
)~";
}; // namespace sql
