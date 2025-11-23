#include <optional>
#include <string>

#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utest/utest.hpp>

#include "s3_secdist.hpp"

using v1::S3CredentialsSecdist;

UTEST(S3Secdist, ParsesAllFields)
{
    userver::formats::json::ValueBuilder builder;
    auto creds = builder["s3_credentials"];
    creds["access_key_id"] = "ACCESS";
    creds["secret_access_key"] = "SECRET";
    creds["session_token"] = "TOKEN";

    const S3CredentialsSecdist parsed(builder.ExtractValue());
    ASSERT_TRUE(parsed.access_key_id);
    ASSERT_TRUE(parsed.secret_access_key);
    ASSERT_TRUE(parsed.session_token);
    EXPECT_EQ(parsed.access_key_id->GetUnderlying(), std::string("ACCESS"));
    EXPECT_EQ(parsed.secret_access_key->GetUnderlying(), std::string("SECRET"));
    EXPECT_EQ(parsed.session_token->GetUnderlying(), std::string("TOKEN"));
}

UTEST(S3Secdist, MissingObjectYieldsNullopts)
{
    userver::formats::json::ValueBuilder builder; // empty root
    const S3CredentialsSecdist parsed(builder.ExtractValue());
    EXPECT_FALSE(parsed.access_key_id);
    EXPECT_FALSE(parsed.secret_access_key);
    EXPECT_FALSE(parsed.session_token);
}

UTEST(S3Secdist, PartialCredentials)
{
    userver::formats::json::ValueBuilder builder;
    auto creds = builder["s3_credentials"];
    creds["access_key_id"] = "ACCESS_ONLY";

    const S3CredentialsSecdist parsed(builder.ExtractValue());
    ASSERT_TRUE(parsed.access_key_id);
    EXPECT_EQ(parsed.access_key_id->GetUnderlying(), std::string("ACCESS_ONLY"));
    EXPECT_FALSE(parsed.secret_access_key);
    EXPECT_FALSE(parsed.session_token);
}
