#include <string>

#include <userver/utest/utest.hpp>

#include "host_policy.hpp"

using v1::HostPolicy::hasSpecialTldSuffix;
using v1::HostPolicy::isBareName;
using v1::HostPolicy::isDeniedHostname;

UTEST(HostPolicy, BareNameDetection)
{
    EXPECT_TRUE(isBareName("localhost"));
    EXPECT_TRUE(isBareName("printer"));
    EXPECT_FALSE(isBareName("example.com"));
}

UTEST(HostPolicy, DeniedHostnames)
{
    EXPECT_TRUE(isDeniedHostname("localhost"));
    EXPECT_TRUE(isDeniedHostname("host.docker.internal"));
    EXPECT_FALSE(isDeniedHostname("example.com"));
}

UTEST(HostPolicy, SpecialTldSuffixesAndPlainNames)
{
    EXPECT_TRUE(hasSpecialTldSuffix("printer.local"));
    EXPECT_TRUE(hasSpecialTldSuffix("local"));
    EXPECT_TRUE(hasSpecialTldSuffix("router.home.arpa"));
    EXPECT_TRUE(hasSpecialTldSuffix("home.arpa"));
    EXPECT_TRUE(hasSpecialTldSuffix("test"));
    EXPECT_TRUE(hasSpecialTldSuffix("invalid"));
    EXPECT_TRUE(hasSpecialTldSuffix("example"));

    EXPECT_FALSE(hasSpecialTldSuffix("example.com"));
    EXPECT_FALSE(hasSpecialTldSuffix("notlocaldomain"));
}
