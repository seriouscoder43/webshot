#pragma once
#include <string_view>

namespace sql {

inline constexpr std::string_view kInsertWebshot = R"~(
insert into webshot(id, created_at, url)
values(default, default, $1)
returning id
)~";

inline constexpr std::string_view kSelectWebshot = R"~(
select id from webshot where id = $1
)~";

inline constexpr std::string_view kSelectWebshotByUrl = R"~(
select id, created_at from webshot where url = $1 order by created_at desc limit $2
)~";

}; // namespace sql
