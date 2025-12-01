#pragma once

#include "s3_credentials_types.hpp"
#include "text.hpp"

#include <chrono>
#include <functional>
#include <string>

#include <userver/clients/http/client.hpp>

namespace v1 {

/**
 * @brief Result of a single STS AssumeRole call for S3 credentials.
 */
struct [[nodiscard]] StsCredentials {
    s3v4::AccessKeyId accessKeyId;
    s3v4::SecretAccessKey secretAccessKey;
    s3v4::SessionToken sessionToken;
    std::chrono::system_clock::time_point expiresAt;

    explicit StsCredentials(const String &xml);
};

/**
 * @brief Call STS AssumeRole at the given endpoint and parse temporary S3
 * credentials.
 *
 * The endpoint must use https. A prebuilt policy JSON is passed verbatim.
 */
[[nodiscard]] StsCredentials fetchStsCredentials(
    userver::clients::http::Client &httpClient, const String &stsEndpoint,
    const s3v4::AccessKeyId &staticAccessKeyId, const s3v4::SecretAccessKey &staticSecretAccessKey,
    const String &region, const String &roleArn, const String &roleSessionName,
    const String &policyJson, std::chrono::seconds duration, std::chrono::milliseconds timeout
);

namespace detail {

using StsExecutor = std::function<std::string(
    const String &url, const String &body, const userver::clients::http::Headers &headers,
    std::chrono::milliseconds timeout
)>;

[[nodiscard]] StsCredentials fetchStsWithExecutor(
    const StsExecutor &exec, const String &stsEndpoint, const s3v4::AccessKeyId &staticAccessKeyId,
    const s3v4::SecretAccessKey &staticSecretAccessKey, const String &region, const String &roleArn,
    const String &roleSessionName, const String &policyJson, std::chrono::seconds duration,
    std::chrono::milliseconds timeout
);

} // namespace detail

} // namespace v1
