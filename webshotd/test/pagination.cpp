#include <string>

#include <userver/utils/boost_uuid4.hpp>

#include <userver/utest/utest.hpp>

#include "pagination.hpp"
#include "text.hpp"

namespace v1 {
namespace us = userver;
} // namespace v1

using namespace v1;

using v1::crud::Clock;
using v1::crud::Cursor;
using v1::crud::DecodeCursor;
using v1::crud::EncodeCursor;
using v1::crud::PageDirection;
using v1::crud::TimePointToMicros;
using namespace text::literals;

UTEST(Pagination, CursorRoundTrip)
{
    Cursor cursor{
        Clock::time_point(std::chrono::microseconds(987654321)),
        us::utils::generators::GenerateBoostUuid(),
        PageDirection::kPrevious,
    };

    const auto token = EncodeCursor(cursor.created_at, cursor.id, cursor.direction);
    const auto decoded = DecodeCursor(token);
    ASSERT_TRUE(decoded);
    if (!decoded)
        return;
    EXPECT_EQ(TimePointToMicros(decoded->created_at), TimePointToMicros(cursor.created_at));
    EXPECT_EQ(decoded->id, cursor.id);
    EXPECT_EQ(decoded->direction, cursor.direction);
}

UTEST(Pagination, DecodeCursorInvalidReturnsNullopt)
{
    const auto decoded = DecodeCursor("invalid-token"_t);
    EXPECT_FALSE(decoded);
}
