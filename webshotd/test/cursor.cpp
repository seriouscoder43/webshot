#include <string>
#include <string_view>

#include <userver/crypto/base64.hpp>
#include <userver/utils/boost_uuid4.hpp>

#include <userver/utest/utest.hpp>

#include "cursor.hpp"
#include "schema/public/webshot.hpp"
#include "text.hpp"

namespace v1 {
namespace us = userver;
} // namespace v1

using namespace v1;

using v1::crud::Clock;
using v1::crud::DecodeToken;
using v1::crud::EncodeToken;
using v1::crud::MicrosToTimePoint;
using v1::crud::TimePointToMicros;
using namespace text::literals;

namespace {

[[nodiscard]] String EncodeRawToken(std::string_view bytes)
{
    return String::FromBytes(
               us::crypto::base64::Base64UrlEncode(bytes, us::crypto::base64::Pad::kWithout)
    )
        .Expect();
}

} // namespace

UTEST(Cursor, TimePointRoundTrip)
{
    const auto tp = Clock::now();
    const auto micros = TimePointToMicros(tp);
    const auto tp2 = MicrosToTimePoint(micros);
    EXPECT_EQ(TimePointToMicros(tp2), micros);
}

UTEST(Cursor, EncodeDecodePaginationCursor)
{
    const auto tp = Clock::time_point(std::chrono::microseconds(123456789));
    const auto micros = TimePointToMicros(tp);
    const auto id = us::utils::generators::GenerateBoostUuid();

    dto::PaginationCursor cur(micros, id, dto::PaginationCursor::D::kNext);
    const auto token = EncodeToken(cur);

    const auto decoded = DecodeToken<dto::PaginationCursor>(token);
    ASSERT_TRUE(decoded);
    if (!decoded)
        return;
    EXPECT_EQ(decoded->t, micros);
    EXPECT_EQ(decoded->i, id);
    EXPECT_EQ(decoded->d, dto::PaginationCursor::D::kNext);
}

UTEST(Cursor, DecodeTokenInvalidReturnsNullopt)
{
    const auto decoded = DecodeToken<dto::PaginationCursor>("not-a-token"_t);
    EXPECT_FALSE(decoded);
}

UTEST(Cursor, DecodeTokenInvalidJsonReturnsNullopt)
{
    const auto decoded = DecodeToken<dto::PaginationCursor>(EncodeRawToken("{"));
    EXPECT_FALSE(decoded);
}

UTEST(Cursor, DecodeTokenInvalidShapeReturnsNullopt)
{
    const auto decoded = DecodeToken<dto::PaginationCursor>(EncodeRawToken("{\"t\":123}"));
    EXPECT_FALSE(decoded);
}
