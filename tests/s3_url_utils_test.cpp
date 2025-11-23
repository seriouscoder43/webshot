#include <string>
#include <utility>
#include <vector>

#include <userver/utest/utest.hpp>

#include "s3/s3_url_utils.hpp"

using v1::s3v4::decodeQueryString;

UTEST(S3UrlUtils, EmptyAndQuestionOnly)
{
    EXPECT_TRUE(decodeQueryString("").empty());
    EXPECT_TRUE(decodeQueryString("?").empty());
}

UTEST(S3UrlUtils, SinglePairAndTrailingAmp)
{
    auto v = decodeQueryString("a=1");
    ASSERT_EQ(v.size(), 1);
    EXPECT_EQ(v[0], (std::pair<std::string, std::string>{"a", "1"}));

    v = decodeQueryString("a=1&");
    ASSERT_EQ(v.size(), 1);
    EXPECT_EQ(v[0], (std::pair<std::string, std::string>{"a", "1"}));
}

UTEST(S3UrlUtils, MultipleAndRepeatedKeys)
{
    auto v = decodeQueryString("a=1&b=2&a=3");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], (std::pair<std::string, std::string>{"a", "1"}));
    EXPECT_EQ(v[1], (std::pair<std::string, std::string>{"b", "2"}));
    EXPECT_EQ(v[2], (std::pair<std::string, std::string>{"a", "3"}));
}

UTEST(S3UrlUtils, LeadingQuestionMark)
{
    auto v = decodeQueryString("?a=1&b=2");
    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0].first, "a");
    EXPECT_EQ(v[0].second, "1");
    EXPECT_EQ(v[1].first, "b");
    EXPECT_EQ(v[1].second, "2");
}

UTEST(S3UrlUtils, PercentDecodingKeyAndValue)
{
    auto v = decodeQueryString("x%20y=hello%20world");
    ASSERT_EQ(v.size(), 1);
    EXPECT_EQ(v[0].first, "x y");
    EXPECT_EQ(v[0].second, "hello world");
}

UTEST(S3UrlUtils, HandlesLeadingSegmentWithoutEquals)
{
    auto v = decodeQueryString("noeq&foo=bar");
    ASSERT_GE(v.size(), 1);
    EXPECT_EQ(v[0].first, "noeq&foo");
    EXPECT_EQ(v[0].second, "bar");
}

UTEST(S3UrlUtils, AllTextNoEqualsYieldsEmpty)
{
    auto v = decodeQueryString("noeq");
    EXPECT_TRUE(v.empty());
}
