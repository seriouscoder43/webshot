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
[[nodiscard]] dto::PaginationCursor::D ToDto(PageDirection direction)
{
    using namespace text::literals;

    switch (direction) {
    case PageDirection::kNext:
        return dto::PaginationCursor::D::kNext;
    case PageDirection::kPrevious:
        return dto::PaginationCursor::D::kPrevious;
    default:
        Invariant("invalid page direction"_t);
    }
}

[[nodiscard]] PageDirection FromDto(dto::PaginationCursor::D direction)
{
    using namespace text::literals;

    switch (direction) {
    case dto::PaginationCursor::D::kNext:
        return PageDirection::kNext;
    case dto::PaginationCursor::D::kPrevious:
        return PageDirection::kPrevious;
    default:
        Invariant("invalid page direction"_t);
    }
}
} // namespace

[[nodiscard]] String
EncodeCursor(Clock::time_point created_at, const Uuid &id, PageDirection direction)
{
    const auto micros = TimePointToMicros(created_at);
    dto::PaginationCursor cur(micros, id, ToDto(direction));
    return EncodeToken(cur);
}

[[nodiscard]] std::optional<Cursor> DecodeCursor(const String &token)
{
    const auto cur = TRY(DecodeToken<dto::PaginationCursor>(token));
    return Cursor{
        .created_at = MicrosToTimePoint(cur.t),
        .id = cur.i,
        .direction = FromDto(cur.d),
    };
}

} // namespace v1::crud
