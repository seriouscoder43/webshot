#include <chrono>
#include <string>

#include <userver/clients/http/response.hpp>
#include <userver/utest/utest.hpp>

#include "s3/s3_sts_client.hpp"
#include "text.hpp"

namespace v1 {
namespace us = userver;
namespace httpc = us::clients::http;
} // namespace v1

using namespace v1;

using namespace std::chrono_literals;
using v1::StsCredentials;
using namespace text::literals;
using text::ToBytes;

namespace {

std::string MakeValidXml()
{
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<AssumeRoleResponse>
  <AssumeRoleResult>
    <Credentials>
      <AccessKeyId>AKIA_TEST_KEY</AccessKeyId>
      <SecretAccessKey>SECRET_TEST_KEY</SecretAccessKey>
      <SessionToken>TOKEN_TEST_VALUE</SessionToken>
      <Expiration>2025-12-01T00:00:00Z</Expiration>
    </Credentials>
  </AssumeRoleResult>
</AssumeRoleResponse>)";
}

} // namespace

UTEST(S3StsClient, ParsesHappyPathXml)
{
    const auto parsed = StsCredentials::FromXml(String::FromBytes(MakeValidXml()).Expect());
    ASSERT_TRUE(parsed);
    const auto &creds = *parsed;

    EXPECT_EQ(creds.access_key_id.GetUnderlying(), "AKIA_TEST_KEY"_t);
    EXPECT_EQ(creds.secret_access_key.GetUnderlying(), "SECRET_TEST_KEY"_t);
    EXPECT_EQ(creds.session_token.GetUnderlying(), "TOKEN_TEST_VALUE"_t);

    using std::chrono::system_clock;
    const auto expected = system_clock::from_time_t(1764547200); // 2025-12-01T00:00:00Z
    EXPECT_EQ(system_clock::to_time_t(creds.expires_at), system_clock::to_time_t(expected));
}

UTEST(S3StsClient, MissingTagThrows)
{
    const std::string xml =
        R"(<AssumeRoleResponse><Credentials></Credentials></AssumeRoleResponse>)";
    const auto parsed = StsCredentials::FromXml(String::FromBytes(xml).Expect());
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.Error(), v1::StsError::kXmlMissingTag);
}

UTEST(S3StsClient, MissingClosingTagThrows)
{
    const std::string xml = R"(<AssumeRoleResponse><AssumeRoleResult><Credentials><AccessKeyId>id)";
    const auto parsed = StsCredentials::FromXml(String::FromBytes(xml).Expect());
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.Error(), v1::StsError::kXmlMissingClosingTag);
}

UTEST(S3StsClient, InvalidExpirationReturnsError)
{
    const std::string xml =
        R"(<?xml version="1.0" encoding="UTF-8"?>
<AssumeRoleResponse>
  <AssumeRoleResult>
    <Credentials>
      <AccessKeyId>AKIA_TEST_KEY</AccessKeyId>
      <SecretAccessKey>SECRET_TEST_KEY</SecretAccessKey>
      <SessionToken>TOKEN_TEST_VALUE</SessionToken>
      <Expiration>not-a-timestamp</Expiration>
    </Credentials>
  </AssumeRoleResult>
</AssumeRoleResponse>)";
    const auto parsed = StsCredentials::FromXml(String::FromBytes(xml).Expect());
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.Error(), v1::StsError::kInvalidExpiration);
}

UTEST(S3StsClient, BuildsRequestWithExecutor)
{
    std::string captured_url;
    std::string captured_body;
    httpc::Headers captured_headers;
    auto captured_timeout = 0ms;

    v1::detail::StsExecutor exec = [&](const String &url, const String &body,
                                       const httpc::Headers &headers,
                                       std::chrono::milliseconds timeout) {
        captured_url = ToBytes(url);
        captured_body = ToBytes(body);
        captured_headers = headers;
        captured_timeout = timeout;
        return MakeValidXml();
    };

    const std::string endpoint = "https://sts.example.com/assume?foo=bar";
    const auto parsed = v1::detail::FetchStsWithExecutor(
        exec, String::FromBytes(endpoint).Expect(), v1::s3v4::AccessKeyId{"AKIA_STATIC"_t},
        v1::s3v4::SecretAccessKey{"SECRET"_t}, "us-east-1"_t,
        "arn:aws:iam::123456789012:role/TestRole"_t, "session-name"_t, R"({"allow":true})"_t, 900s,
        1500ms
    );
    ASSERT_TRUE(parsed);
    const auto &creds = *parsed;

    EXPECT_EQ(captured_url, endpoint);
    const std::string auth_key = "authorization";
    const std::string amz_date_key = "x-amz-date";
    const std::string content_type_key = "content-type";
    const std::string host_key = "host";
    EXPECT_TRUE(captured_headers.contains(auth_key));
    EXPECT_TRUE(captured_headers.contains(amz_date_key));
    EXPECT_EQ(captured_headers.at(content_type_key), "application/x-www-form-urlencoded");
    EXPECT_EQ(captured_headers.at(host_key), std::string("sts.example.com"));
    EXPECT_EQ(captured_timeout, 1500ms);

    EXPECT_TRUE(captured_body.contains("Action=AssumeRole"));
    EXPECT_TRUE(captured_body.contains("Version=2011-06-15"));
    EXPECT_TRUE(
        captured_body.contains("RoleArn=arn%3Aaws%3Aiam%3A%3A123456789012%3Arole%2FTestRole")
    );
    EXPECT_TRUE(captured_body.contains("RoleSessionName=session-name"));
    EXPECT_TRUE(captured_body.contains("DurationSeconds=900"));
    EXPECT_TRUE(captured_body.contains("Policy=%7B%22allow%22%3Atrue%7D"));

    EXPECT_EQ(creds.access_key_id.GetUnderlying(), "AKIA_TEST_KEY"_t);
}

UTEST(S3StsClient, InvalidEndpointReturnsError)
{
    v1::detail::StsExecutor exec =
        [](const String &, const String &, const httpc::Headers &,
           std::chrono::milliseconds) -> v1::Expected<std::string, v1::StsError> {
        ADD_FAILURE() << "executor should not be called for invalid endpoint";
        return std::string{};
    };

    const auto parsed = v1::detail::FetchStsWithExecutor(
        exec, "https://["_t, v1::s3v4::AccessKeyId{"AKIA_STATIC"_t},
        v1::s3v4::SecretAccessKey{"SECRET"_t}, "us-east-1"_t,
        "arn:aws:iam::123456789012:role/TestRole"_t, "session-name"_t, R"({"allow":true})"_t, 900s,
        1500ms
    );
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.Error(), v1::StsError::kInvalidEndpoint);
}
