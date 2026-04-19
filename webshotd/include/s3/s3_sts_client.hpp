#pragma once

#include "expected.hpp"
#include "s3_credentials_types.hpp"
#include "text.hpp"
#include "userver_namespaces.hpp"

#include <chrono>
#include <functional>
#include <string>

#include <userver/clients/http/client.hpp>

namespace v1 {

enum class StsError {
    kInvalidEndpoint,
    kInvalidQuery,
    kHttpFailure,
    kXmlMissingTag,
    kXmlMissingClosingTag,
    kInvalidExpiration,
    kInvalidUtf8,
};

/**
 * @brief Result of a single STS AssumeRole call for S3 credentials.
 */
struct [[nodiscard]] StsCredentials {
    s3v4::AccessKeyId accessKeyId;
    s3v4::SecretAccessKey secretAccessKey;
    s3v4::SessionToken sessionToken;
    std::chrono::system_clock::time_point expiresAt;

    [[nodiscard]] static Expected<StsCredentials, StsError> fromXml(const String &xml);
};

/**
 * @brief Call STS AssumeRole at the given endpoint and parse temporary S3
 * credentials.
 *
 * The endpoint must use https. A prebuilt policy JSON is passed verbatim.
 */
[[nodiscard]] Expected<StsCredentials, StsError> fetchStsCredentials(
    httpc::Client &httpClient, const String &stsEndpoint,
    const s3v4::AccessKeyId &staticAccessKeyId, const s3v4::SecretAccessKey &staticSecretAccessKey,
    const String &region, const String &roleArn, const String &roleSessionName,
    const String &policyJson, std::chrono::seconds duration, std::chrono::milliseconds timeout
);

namespace detail {

using StsExecutor = std::function<Expected<std::string, StsError>(
    const String &url, const String &body, const httpc::Headers &headers,
    std::chrono::milliseconds timeout
)>;

[[nodiscard]] Expected<StsCredentials, StsError> fetchStsWithExecutor(
    const StsExecutor &exec, const String &stsEndpoint, const s3v4::AccessKeyId &staticAccessKeyId,
    const s3v4::SecretAccessKey &staticSecretAccessKey, const String &region, const String &roleArn,
    const String &roleSessionName, const String &policyJson, std::chrono::seconds duration,
    std::chrono::milliseconds timeout
);

} // namespace detail

} // namespace v1
