#include <array>
#include <string_view>

#include <arpa/inet.h>

#include <userver/utest/utest.hpp>

#include "ip_utils.hpp"

namespace {

uint32_t parseIpv4(const char *dotted)
{
    in_addr addr4{};
    EXPECT_EQ(inet_pton(AF_INET, dotted, &addr4), 1);
    return ntohl(addr4.s_addr);
}

in6_addr parseIpv6(const char *text)
{
    in6_addr addr6{};
    EXPECT_EQ(inet_pton(AF_INET6, text, &addr6), 1);
    return addr6;
}

} // namespace

UTEST(IpUtils, IsIpLiteralHostnameBasic)
{
    EXPECT_FALSE(IpUtils::isIpLiteralHostname(""));
    EXPECT_TRUE(IpUtils::isIpLiteralHostname("203.0.113.5"));
    EXPECT_TRUE(IpUtils::isIpLiteralHostname("[2001:db8::1]"));
    EXPECT_FALSE(IpUtils::isIpLiteralHostname("not-an-ip"));
    EXPECT_FALSE(IpUtils::isIpLiteralHostname("[invalid]"));
}

UTEST(IpUtils, PublicRoutableIPv4BlocksSpecialRanges)
{
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("0.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("10.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("100.64.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("127.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("169.254.1.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("172.16.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("192.168.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("198.18.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("224.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv4(parseIpv4("240.0.0.1")));
}

UTEST(IpUtils, PublicRoutableIPv4AllowsPublicRange)
{
    EXPECT_TRUE(IpUtils::isPublicRoutableIPv4(parseIpv4("203.0.113.1")));
}

UTEST(IpUtils, PublicRoutableIPv6BlocksSpecialRanges)
{
    auto zero = in6_addr{};
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(zero));

    const auto loopback = parseIpv6("::1");
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(loopback));

    const auto ula = parseIpv6("fc00::1");
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(ula));

    const auto linkLocal = parseIpv6("fe80::1");
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(linkLocal));

    const auto v4mapped = parseIpv6("::ffff:192.0.2.1");
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(v4mapped));

    const auto multicast = parseIpv6("ff00::1");
    EXPECT_FALSE(IpUtils::isPublicRoutableIPv6(multicast));
}

UTEST(IpUtils, PublicRoutableIPv6AllowsGlobalUnicast)
{
    const auto global = parseIpv6("2001:db8::1");
    EXPECT_TRUE(IpUtils::isPublicRoutableIPv6(global));
}
