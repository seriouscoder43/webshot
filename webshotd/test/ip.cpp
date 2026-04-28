#include "ip.hpp"

#include <variant>

#include <userver/utest/utest.hpp>

namespace {

using namespace text::literals;

[[nodiscard]] v1::Ip4 parseIp4Strict(const String &text)
{
    auto ip = v1::parseIp4(text);
    EXPECT_TRUE(ip);
    return *ip;
}

[[nodiscard]] v1::Ip6 parseIp6Strict(const String &text)
{
    auto ip = v1::parseIp6(text);
    EXPECT_TRUE(ip);
    return *ip;
}

[[nodiscard]] v1::Ip parseIpStrict(const String &text)
{
    auto ip = v1::parseIp(text);
    EXPECT_TRUE(ip);
    return *ip;
}

} // namespace

UTEST(Ip, ParsesIpLiteralsIntoTypedValues)
{
    EXPECT_TRUE(v1::parseIp4("203.0.113.1"_t));
    EXPECT_TRUE(v1::parseIp6("2001:db8::1"_t));
    EXPECT_TRUE(v1::parseIp6("[2001:db8::1]"_t));
    EXPECT_TRUE(std::holds_alternative<v1::Ip4>(parseIpStrict("203.0.113.1"_t)));
    EXPECT_TRUE(std::holds_alternative<v1::Ip6>(parseIpStrict("2001:db8::1"_t)));
}

UTEST(Ip, FormatsCanonicalIpText)
{
    EXPECT_EQ(*v1::toCanonicalIpText(parseIpStrict("203.0.113.1"_t)), "203.0.113.1"_t);
    EXPECT_EQ(*v1::toCanonicalIpText(parseIpStrict("[2001:db8::1]"_t)), "2001:db8::1"_t);
    EXPECT_EQ(
        *v1::toCanonicalIpText(parseIpStrict("::ffff:198.51.100.9"_t)), "::ffff:198.51.100.9"_t
    );
}

UTEST(Ip, RejectsInvalidIpText)
{
    EXPECT_FALSE(v1::parseIp(""_t));
    EXPECT_FALSE(v1::parseIp("example.com"_t));
    EXPECT_FALSE(v1::parseIp("127.0.0.1:80"_t));
    EXPECT_FALSE(v1::parseIp("[2001:db8::1"_t));
}

UTEST(Ip, PublicRoutableIpv4BlocksSpecialRanges)
{
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("0.0.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("10.0.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("100.64.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("127.0.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("169.254.169.254"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("172.16.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("192.168.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("198.18.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("224.0.0.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp4Strict("240.0.0.1"_t)));
}

UTEST(Ip, PublicRoutableIpv4AllowsPublicRange)
{
    EXPECT_TRUE(v1::isPublicRoutable(parseIp4Strict("8.8.8.8"_t)));
}

UTEST(Ip, PublicRoutableIpv6BlocksSpecialRanges)
{
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("::"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("::1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("fc00::1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("fe80::1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("::ffff:192.0.2.1"_t)));
    EXPECT_FALSE(v1::isPublicRoutable(parseIp6Strict("ff00::1"_t)));
}

UTEST(Ip, PublicRoutableIpv6AllowsGlobalUnicast)
{
    EXPECT_TRUE(v1::isPublicRoutable(parseIp6Strict("2606:4700:4700::1111"_t)));
}
