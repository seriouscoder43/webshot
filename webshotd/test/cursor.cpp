#include <string>
#include <string_view>

#include "userver_namespaces.hpp"

#include <userver/crypto/base64.hpp>
#include <userver/utils/boost_uuid4.hpp>

#include <userver/utest/utest.hpp>

#include "cursor.hpp"
#include "schema/public/webshot.hpp"
#include "text.hpp"

using v1::crud::Clock;
using v1::crud::decodeToken;
using v1::crud::encodeToken;
using v1::crud::microsToTimePoint;
using v1::crud::timePointToMicros;
using namespace text::literals;

namespace {

[[nodiscard]] String encodeRawToken(std::string_view bytes)
{
    return String::fromBytes(
               us::crypto::base64::Base64UrlEncode(bytes, us::crypto::base64::Pad::kWithout)
    )
        .expect();
}

} // namespace

UTEST(Cursor, TimePointRoundTrip)
{
    const auto tp = Clock::now();
    const auto micros = timePointToMicros(tp);
    const auto tp2 = microsToTimePoint(micros);
    EXPECT_EQ(timePointToMicros(tp2), micros);
}

UTEST(Cursor, EncodeDecodePaginationCursor)
{
    const auto tp = Clock::time_point(std::chrono::microseconds(123456789));
    const auto micros = timePointToMicros(tp);
    const auto id = us::utils::generators::GenerateBoostUuid();

    dto::PaginationCursor cur(micros, id, dto::PaginationCursor::D::kNext);
    const auto token = encodeToken(cur);

    const auto decoded = decodeToken<dto::PaginationCursor>(token);
    ASSERT_TRUE(decoded);
    if (!decoded)
        return;
    EXPECT_EQ(decoded->t, micros);
    EXPECT_EQ(decoded->i, id);
    EXPECT_EQ(decoded->d, dto::PaginationCursor::D::kNext);
}

UTEST(Cursor, DecodeTokenInvalidReturnsNullopt)
{
    const auto decoded = decodeToken<dto::PaginationCursor>("not-a-token"_t);
    EXPECT_FALSE(decoded);
}

UTEST(Cursor, DecodeTokenInvalidJsonReturnsNullopt)
{
    const auto decoded = decodeToken<dto::PaginationCursor>(encodeRawToken("{"));
    EXPECT_FALSE(decoded);
}

UTEST(Cursor, DecodeTokenInvalidShapeReturnsNullopt)
{
    const auto decoded = decodeToken<dto::PaginationCursor>(encodeRawToken("{\"t\":123}"));
    EXPECT_FALSE(decoded);
}
