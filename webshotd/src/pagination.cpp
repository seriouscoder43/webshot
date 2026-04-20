/**
 * @file
 * @brief Implementation of helpers for pagination by exact link.
 */

#include "pagination.hpp"

#include "schema/webshot.hpp"
#include "try.hpp"

namespace v1::crud {

[[nodiscard]] String encodeCursor(const Cursor &cursor)
{
    const auto micros = timePointToMicros(cursor.createdAt);
    dto::PaginationCursor cur(micros, cursor.id);
    return encodeToken(cur);
}

[[nodiscard]] std::optional<Cursor> decodeCursor(const String &token)
{
    const auto cur = TRY(decodeToken<dto::PaginationCursor>(token));
    Cursor out;
    out.createdAt = microsToTimePoint(cur.t);
    out.id = cur.i;
    return out;
}

} // namespace v1::crud
