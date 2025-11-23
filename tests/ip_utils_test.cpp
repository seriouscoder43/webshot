#include <array>
#include <string_view>

#include <arpa/inet.h>

#include <userver/utest/utest.hpp>

#include "ip_utils.hpp"

namespace {

in_addr parseIpv4(const char *dotted)
{
    in_addr addr4{};
    EXPECT_EQ(inet_pton(AF_INET, dotted, &addr4), 1);
    return addr4;
}

in6_addr parseIpv6(const char *text)
{
    in6_addr addr6{};
    EXPECT_EQ(inet_pton(AF_INET6, text, &addr6), 1);
    return addr6;
}

} // namespace

UTEST(IpUtils, PublicIpv4BlocksSpecialRanges)
{
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("0.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("10.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("100.64.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("127.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("169.254.1.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("172.16.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("192.168.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("198.18.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("224.0.0.1")));
    EXPECT_FALSE(IpUtils::isPublicIpv4(parseIpv4("240.0.0.1")));
}

UTEST(IpUtils, PublicIpv4AllowsPublicRange)
{
    EXPECT_TRUE(IpUtils::isPublicIpv4(parseIpv4("203.0.113.1")));
}

UTEST(IpUtils, PublicIpv6BlocksSpecialRanges)
{
    auto zero = in6_addr{};
    EXPECT_FALSE(IpUtils::isPublicIpv6(zero));

    const auto loopback = parseIpv6("::1");
    EXPECT_FALSE(IpUtils::isPublicIpv6(loopback));

    const auto ula = parseIpv6("fc00::1");
    EXPECT_FALSE(IpUtils::isPublicIpv6(ula));

    const auto linkLocal = parseIpv6("fe80::1");
    EXPECT_FALSE(IpUtils::isPublicIpv6(linkLocal));

    const auto v4mapped = parseIpv6("::ffff:192.0.2.1");
    EXPECT_FALSE(IpUtils::isPublicIpv6(v4mapped));

    const auto multicast = parseIpv6("ff00::1");
    EXPECT_FALSE(IpUtils::isPublicIpv6(multicast));
}

UTEST(IpUtils, PublicIpv6AllowsGlobalUnicast)
{
    const auto global = parseIpv6("2001:db8::1");
    EXPECT_TRUE(IpUtils::isPublicIpv6(global));
}
