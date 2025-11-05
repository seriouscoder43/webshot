#include <string>

#include <ada.h>
#include <userver/utest/utest.hpp>

#include "common_definitions.hpp"
#include "url_validation.hpp"

UTEST(IsValidWebshotUrl, AcceptsHttpsWithHostname)
{
    auto parsed = ada::parse<ada::url_aggregator>("https://example.com/");
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(isValidForWebshotUrl(*parsed));
}

UTEST(IsValidWebshotUrl, RejectsUnsupportedScheme)
{
    auto parsed = ada::parse<ada::url_aggregator>("ftp://example.com/");
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(isValidForWebshotUrl(*parsed));
}

UTEST(IsValidWebshotUrl, RejectsMissingHostname)
{
    ada::url_aggregator url;
    url.type = ada::scheme::type::HTTP;
    EXPECT_FALSE(url.has_hostname());
    EXPECT_TRUE(url.get_hostname().empty());
    EXPECT_FALSE(isValidForWebshotUrl(url));
}

UTEST(IsValidWebshotUrl, AcceptsQueryAtLimit)
{
    std::string url_string = "https://example.com/?";
    url_string.append(v1::kQueryPartLengthMax - 1, 'a');
    auto parsed = ada::parse<ada::url_aggregator>(url_string);
    ASSERT_TRUE(parsed);
    EXPECT_LE(parsed->get_search().size(), v1::kQueryPartLengthMax);
    EXPECT_TRUE(isValidForWebshotUrl(*parsed));
}

UTEST(IsValidWebshotUrl, RejectsQueryOverLimit)
{
    std::string url_string = "https://example.com/?";
    url_string.append(v1::kQueryPartLengthMax + 1, 'a');
    auto parsed = ada::parse<ada::url_aggregator>(url_string);
    ASSERT_TRUE(parsed);
    EXPECT_GT(parsed->get_search().size(), v1::kQueryPartLengthMax);
    EXPECT_FALSE(isValidForWebshotUrl(*parsed));
}
