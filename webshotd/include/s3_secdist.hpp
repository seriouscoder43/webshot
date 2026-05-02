#pragma once

#include "text.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <userver/formats/json/value.hpp>
#include <userver/utils/assert.hpp>

#include "s3_credentials_types.hpp"

namespace v1 {

namespace us = userver;
namespace json = us::formats::json;
namespace detail {

template <typename Credential>
[[nodiscard]] std::optional<Credential>
CredentialTextField(const json::Value &creds, std::string_view field_name)
{
    const auto value = creds[std::string{field_name}];
    if (value.IsMissing())
        return {};
    return Credential(String::FromBytes(value.As<std::string>()).Expect());
}

} // namespace detail

/**
 * @brief Light wrapper to read S3 credentials from secdist.
 *
 * This type looks for the `s3_credentials` object with `access_key_id` and
 * `secret_access_key` fields and exposes them as optionals. An optional
 * `session_token` is also supported for temporary credentials.
 */
struct S3CredentialsSecdist {
    std::optional<s3v4::AccessKeyId> access_key_id;
    std::optional<s3v4::SecretAccessKey> secret_access_key;
    std::optional<s3v4::SessionToken> session_token;

    explicit S3CredentialsSecdist(const json::Value &secdist_doc)
    {
        const auto creds = secdist_doc["s3_credentials"];
        if (!creds.IsMissing()) {
            access_key_id = detail::CredentialTextField<s3v4::AccessKeyId>(creds, "access_key_id");
            secret_access_key = detail::CredentialTextField<s3v4::SecretAccessKey>(
                creds, "secret_access_key"
            );
            session_token = detail::CredentialTextField<s3v4::SessionToken>(creds, "session_token");
        }
    }
};

} // namespace v1
