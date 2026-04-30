/**
 * @file
 * @brief Implementation of helpers for pagination by link prefix.
 */

#include "prefix_pagination.hpp"

#include "integers.hpp"
#include "invariant.hpp"
#include "schema/public/webshot.hpp"
#include "text.hpp"
#include "try.hpp"

#include <userver/utils/assert.hpp>

namespace v1::crud {

using namespace text::literals;
using text::toBytes;

namespace {
[[nodiscard]] dto::PaginationPrefixCursor::D toDto(PageDirection direction)
{
    switch (direction) {
    case PageDirection::kNext:
        return dto::PaginationPrefixCursor::D::kNext;
    case PageDirection::kPrevious:
        return dto::PaginationPrefixCursor::D::kPrevious;
    default:
        invariant("invalid page direction"_t);
    }
}

[[nodiscard]] PageDirection fromDto(dto::PaginationPrefixCursor::D direction)
{
    switch (direction) {
    case dto::PaginationPrefixCursor::D::kNext:
        return PageDirection::kNext;
    case dto::PaginationPrefixCursor::D::kPrevious:
        return PageDirection::kPrevious;
    default:
        invariant("invalid page direction"_t);
    }
}
} // namespace

[[nodiscard]] std::optional<PrefixCursor> decodePrefixCursor(const String &token)
{
    const auto cur = TRY(decodeToken<dto::PaginationPrefixCursor>(token));
    PrefixCursor out{};
    out.prefix = TRY(String::fromBytes(cur.p));
    out.link = TRY(String::fromBytes(cur.l));
    out.direction = fromDto(cur.d);
    if (cur.t && cur.i) {
        out.createdAt = microsToTimePoint(*cur.t);
        out.id = *cur.i;
    }
    return out;
}

[[nodiscard]] String
encodePrefixCursor(const String &prefix, const String &link, PageDirection direction)
{
    dto::PaginationPrefixCursor cur(toBytes(prefix), toBytes(link), toDto(direction));
    return encodeToken(cur);
}

[[nodiscard]] String encodePrefixCursor(
    const String &prefix, const String &link, Clock::time_point createdAt, const Uuid &id,
    PageDirection direction
)
{
    const auto micros = timePointToMicros(createdAt);
    dto::PaginationPrefixCursor cur(toBytes(prefix), toBytes(link), toDto(direction), micros, id);
    return encodeToken(cur);
}

[[nodiscard]] std::string upperExclusiveBound(String s)
{
    invariant(!s.empty(), "cannot be empty"_t);
    auto view = s.view();
    std::string bytes(view);
    for (i64 i = ssize(bytes) - 1_i64; i >= 0_i64; i--) {
        const auto j = numericCast<size_t>(i);
        unsigned char c = static_cast<unsigned char>(bytes[j]);
        if (c < 0xFF) {
            bytes[j] = static_cast<char>(c + 1);
            bytes.resize(j + 1);
            break;
        }
    }
    return bytes;
}

} // namespace v1::crud
