#include <optional>
#include <string>

#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utest/utest.hpp>

#include "s3_secdist.hpp"
#include "text.hpp"

namespace v1 {
namespace us = userver;
namespace json = us::formats::json;
} // namespace v1

using namespace v1;

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
    ASSERT_TRUE(parsed.access_key_id);
    ASSERT_TRUE(parsed.secret_access_key);
    ASSERT_TRUE(parsed.session_token);
    if (!parsed.access_key_id || !parsed.secret_access_key || !parsed.session_token)
        return;
    EXPECT_EQ(parsed.access_key_id->GetUnderlying(), "ACCESS"_t);
    EXPECT_EQ(parsed.secret_access_key->GetUnderlying(), "SECRET"_t);
    EXPECT_EQ(parsed.session_token->GetUnderlying(), "TOKEN"_t);
}

UTEST(S3Secdist, MissingObjectYieldsNullopts)
{
    json::ValueBuilder builder; // empty root
    const S3CredentialsSecdist parsed(builder.ExtractValue());
    EXPECT_FALSE(parsed.access_key_id);
    EXPECT_FALSE(parsed.secret_access_key);
    EXPECT_FALSE(parsed.session_token);
}

UTEST(S3Secdist, PartialCredentials)
{
    json::ValueBuilder builder;
    auto creds = builder["s3_credentials"];
    creds["access_key_id"] = "ACCESS_ONLY";

    const S3CredentialsSecdist parsed(builder.ExtractValue());
    ASSERT_TRUE(parsed.access_key_id);
    if (!parsed.access_key_id)
        return;
    EXPECT_EQ(parsed.access_key_id->GetUnderlying(), "ACCESS_ONLY"_t);
    EXPECT_FALSE(parsed.secret_access_key);
    EXPECT_FALSE(parsed.session_token);
}
