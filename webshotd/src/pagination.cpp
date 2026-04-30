/**
 * @file
 * @brief Implementation of helpers for pagination by exact link.
 */

#include "pagination.hpp"

#include "invariant.hpp"
#include "schema/public/webshot.hpp"
#include "text.hpp"
#include "try.hpp"

namespace v1::crud {

namespace {
[[nodiscard]] dto::PaginationCursor::D toDto(PageDirection direction)
{
    using namespace text::literals;

    switch (direction) {
    case PageDirection::kNext:
        return dto::PaginationCursor::D::kNext;
    case PageDirection::kPrevious:
        return dto::PaginationCursor::D::kPrevious;
    default:
        invariant("invalid page direction"_t);
    }
}

[[nodiscard]] PageDirection fromDto(dto::PaginationCursor::D direction)
{
    using namespace text::literals;

    switch (direction) {
    case dto::PaginationCursor::D::kNext:
        return PageDirection::kNext;
    case dto::PaginationCursor::D::kPrevious:
        return PageDirection::kPrevious;
    default:
        invariant("invalid page direction"_t);
    }
}
} // namespace

[[nodiscard]] String
encodeCursor(Clock::time_point createdAt, const Uuid &id, PageDirection direction)
{
    const auto micros = timePointToMicros(createdAt);
    dto::PaginationCursor cur(micros, id, toDto(direction));
    return encodeToken(cur);
}

[[nodiscard]] std::optional<Cursor> decodeCursor(const String &token)
{
    const auto cur = TRY(decodeToken<dto::PaginationCursor>(token));
    return Cursor{
        .createdAt = microsToTimePoint(cur.t),
        .id = cur.i,
        .direction = fromDto(cur.d),
    };
}

} // namespace v1::crud
