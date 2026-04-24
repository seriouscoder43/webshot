#pragma once

#include "text.hpp"
#include "userver_namespaces.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <userver/formats/json/value.hpp>
#include <userver/utils/assert.hpp>

#include "s3_credentials_types.hpp"

namespace v1 {

namespace detail {

template <typename Credential>
[[nodiscard]] std::optional<Credential>
credentialTextField(const json::Value &creds, std::string_view fieldName)
{
    const auto value = creds[std::string{fieldName}];
    if (value.IsMissing())
        return {};
    return Credential(String::fromBytes(value.As<std::string>()).expect());
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
    std::optional<s3v4::AccessKeyId> accessKeyId;
    std::optional<s3v4::SecretAccessKey> secretAccessKey;
    std::optional<s3v4::SessionToken> sessionToken;

    explicit S3CredentialsSecdist(const json::Value &secdistDoc)
    {
        const auto creds = secdistDoc["s3_credentials"];
        if (!creds.IsMissing()) {
            accessKeyId = detail::credentialTextField<s3v4::AccessKeyId>(creds, "access_key_id");
            secretAccessKey = detail::credentialTextField<s3v4::SecretAccessKey>(
                creds, "secret_access_key"
            );
            sessionToken = detail::credentialTextField<s3v4::SessionToken>(creds, "session_token");
        }
    }
};

} // namespace v1
