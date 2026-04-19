#include <optional>
#include <string>

#include "userver_namespaces.hpp"

#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utest/utest.hpp>

#include "s3_secdist.hpp"
#include "text.hpp"

using v1::S3CredentialsSecdist;
using namespace text::literals;

UTEST(S3Secdist, ParsesAllFields)
{
    json::ValueBuilder builder;
    auto creds = builder["s3_credentials"];
    creds["access_key_id"] = "ACCESS";
    creds["secret_access_key"] = "SECRET";
    creds["session_token"] = "TOKEN";

    const S3CredentialsSecdist parsed(builder.ExtractValue());
    ASSERT_TRUE(parsed.accessKeyId);
    ASSERT_TRUE(parsed.secretAccessKey);
    ASSERT_TRUE(parsed.sessionToken);
    if (!parsed.accessKeyId || !parsed.secretAccessKey || !parsed.sessionToken)
        return;
    EXPECT_EQ(parsed.accessKeyId->GetUnderlying(), "ACCESS"_t);
    EXPECT_EQ(parsed.secretAccessKey->GetUnderlying(), "SECRET"_t);
    EXPECT_EQ(parsed.sessionToken->GetUnderlying(), "TOKEN"_t);
}

UTEST(S3Secdist, MissingObjectYieldsNullopts)
{
    json::ValueBuilder builder; // empty root
    const S3CredentialsSecdist parsed(builder.ExtractValue());
    EXPECT_FALSE(parsed.accessKeyId);
    EXPECT_FALSE(parsed.secretAccessKey);
    EXPECT_FALSE(parsed.sessionToken);
}

UTEST(S3Secdist, PartialCredentials)
{
    json::ValueBuilder builder;
    auto creds = builder["s3_credentials"];
    creds["access_key_id"] = "ACCESS_ONLY";

    const S3CredentialsSecdist parsed(builder.ExtractValue());
    ASSERT_TRUE(parsed.accessKeyId);
    if (!parsed.accessKeyId)
        return;
    EXPECT_EQ(parsed.accessKeyId->GetUnderlying(), "ACCESS_ONLY"_t);
    EXPECT_FALSE(parsed.secretAccessKey);
    EXPECT_FALSE(parsed.sessionToken);
}
