#include <string>

#include <userver/utest/utest.hpp>

#include "host_policy.hpp"

using v1::HostPolicy::HasSpecialTldSuffix;
using v1::HostPolicy::IsBareName;
using v1::HostPolicy::IsDeniedHostname;

UTEST(HostPolicy, BareNameDetection)
{
    EXPECT_TRUE(IsBareName("localhost"));
    EXPECT_TRUE(IsBareName("printer"));
    EXPECT_FALSE(IsBareName("example.com"));
}

UTEST(HostPolicy, DeniedHostnames)
{
    EXPECT_TRUE(IsDeniedHostname("localhost"));
    EXPECT_TRUE(IsDeniedHostname("host.docker.internal"));
    EXPECT_FALSE(IsDeniedHostname("example.com"));
}

UTEST(HostPolicy, SpecialTldSuffixesAndPlainNames)
{
    EXPECT_TRUE(HasSpecialTldSuffix("printer.local"));
    EXPECT_TRUE(HasSpecialTldSuffix("local"));
    EXPECT_TRUE(HasSpecialTldSuffix("router.home.arpa"));
    EXPECT_TRUE(HasSpecialTldSuffix("home.arpa"));
    EXPECT_TRUE(HasSpecialTldSuffix("test"));
    EXPECT_TRUE(HasSpecialTldSuffix("invalid"));
    EXPECT_TRUE(HasSpecialTldSuffix("example"));

    EXPECT_FALSE(HasSpecialTldSuffix("example.com"));
    EXPECT_FALSE(HasSpecialTldSuffix("notlocaldomain"));
}
