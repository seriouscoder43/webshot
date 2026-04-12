#include "s3/s3_sts_client.hpp"

#include "integers.hpp"
#include "link.hpp"
#include "s3/s3_url_utils.hpp"
#include "s3/sigv4_signer.hpp"
#include "s3_credentials_types.hpp"
#include "text.hpp"

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <userver/clients/http/client.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/datetime/from_string_saturating.hpp>

namespace http = userver::clients::http;
using namespace text::literals;

namespace v1 {

namespace {

[[nodiscard]] Expected<String, StsError> extractXmlTag(const String &xml, String tag) noexcept
{
    std::string open = "<";
    std::string xmlBytes(xml.view()), tagBytes(tag.view());
    open.append(tagBytes.data(), tagBytes.size()).push_back('>');
    std::string close = "</";
    close.append(tagBytes.data(), tagBytes.size()).push_back('>');
    const auto startPos = xmlBytes.find(open);
    if (startPos == std::string::npos)
        return std::unexpected(StsError::kXmlMissingTag);
    const auto valuePos = startPos + open.size();
    const auto endPos = xmlBytes.find(close, valuePos);
    if (endPos == std::string::npos)
        return std::unexpected(StsError::kXmlMissingClosingTag);
    const auto extracted = String::fromBytes(xmlBytes.substr(valuePos, endPos - valuePos));
    if (!extracted)
        return std::unexpected(StsError::kInvalidUtf8);
    return extracted.value();
}

} // namespace

Expected<StsCredentials, StsError> StsCredentials::fromXml(const String &xml) noexcept
{
    auto accessKeyId = extractXmlTag(xml, "AccessKeyId"_t);
    if (!accessKeyId)
        return std::unexpected(accessKeyId.error());
    auto secretAccessKey = extractXmlTag(xml, "SecretAccessKey"_t);
    if (!secretAccessKey)
        return std::unexpected(secretAccessKey.error());
    auto sessionToken = extractXmlTag(xml, "SessionToken"_t);
    if (!sessionToken)
        return std::unexpected(sessionToken.error());
    auto expiration = extractXmlTag(xml, "Expiration"_t);
    if (!expiration)
        return std::unexpected(expiration.error());

    StsCredentials creds{
        s3v4::AccessKeyId(std::move(accessKeyId).value()),
        s3v4::SecretAccessKey(std::move(secretAccessKey).value()),
        s3v4::SessionToken(std::move(sessionToken).value()),
        userver::utils::datetime::FromRfc3339StringSaturating(std::string(expiration->view())),
    };
    return creds;
}

Expected<StsCredentials, StsError> detail::fetchStsWithExecutor(
    const StsExecutor &exec, const String &stsEndpoint, const s3v4::AccessKeyId &staticAccessKeyId,
    const s3v4::SecretAccessKey &staticSecretAccessKey, const String &region, const String &roleArn,
    const String &roleSessionName, const String &policyJson, std::chrono::seconds duration,
    std::chrono::milliseconds timeout
)
{
    // Link::fromText may insert "http://" when scheme is absent; allow that overhead.
    constexpr size_t kDefaultSchemeBytes = 7UL; // "http://"
    const auto stsLink = Link::fromText(
                             stsEndpoint, stsEndpoint.sizeBytes() + kDefaultSchemeBytes,
                             Link::FromTextOptions::kNone
    )
                             .expect();
    UINVARIANT(stsLink.url.isHttps(), "STS endpoint must use https scheme");

    const auto host = stsLink.url.host();

    auto path = stsLink.url.pathname();
    if (path.empty())
        path = "/"_t;

    std::vector<std::pair<String, String>> query;
    if (stsLink.url.hasSearch()) {
        const auto search = stsLink.url.search();
        auto decoded = s3v4::decodeQueryString(search);
        if (!decoded)
            return std::unexpected(StsError::kInvalidQuery);
        query = std::move(decoded).value();
    }
    String body;
    auto appendParam = [&body](const String &name, const String &value) {
        if (!body.empty())
            body += "&"_t;
        body += name;
        body += "="_t;
        body += s3v4::percentEncode(value, s3v4::EncodeSlash::kYes);
    };
    appendParam("Action"_t, "AssumeRole"_t);
    appendParam("Version"_t, "2011-06-15"_t);
    appendParam("RoleArn"_t, roleArn);
    appendParam("RoleSessionName"_t, roleSessionName);
    appendParam("DurationSeconds"_t, text::format("{}", duration.count()));
    appendParam("Policy"_t, policyJson);

    const String payloadHash = s3v4::sha256Hex(body.view());

    const auto now = userver::utils::datetime::Now();
    s3v4::SigV4Params params(
        std::string(region.view()), "sts", staticAccessKeyId, staticSecretAccessKey, {}, now
    );

    std::vector<std::pair<String, String>> headersToSign;
    headersToSign.emplace_back("host"_t, host);
    const auto kUrlEncoded = "application/x-www-form-urlencoded"_t;
    headersToSign.emplace_back("content-type"_t, kUrlEncoded);
    const auto signedHeaders = s3v4::signHeaders(
        params, "POST"_t, path, query, headersToSign, payloadHash
    );
    http::Headers headers;
    headers[userver::http::headers::kHost] = std::string(host.view());
    headers[userver::http::headers::kContentType] = std::string(kUrlEncoded.view());
    for (const auto &kv : signedHeaders)
        headers[kv.first] = kv.second;
    const auto response = exec(stsLink.httpsUrl(), body, headers, timeout);
    if (!response)
        return std::unexpected(response.error());

    const auto responseXml = String::fromBytes(response.value());
    if (!responseXml)
        return std::unexpected(StsError::kInvalidUtf8);
    return StsCredentials::fromXml(responseXml.value());
}

Expected<StsCredentials, StsError> fetchStsCredentials(
    http::Client &httpClient, const String &stsEndpoint, const s3v4::AccessKeyId &staticAccessKeyId,
    const s3v4::SecretAccessKey &staticSecretAccessKey, const String &region, const String &roleArn,
    const String &roleSessionName, const String &policyJson, std::chrono::seconds duration,
    std::chrono::milliseconds timeout
)
{
    detail::StsExecutor exec = [&httpClient](
                                   const String &url, const String &body,
                                   const http::Headers &headers, std::chrono::milliseconds timeoutMs
                               ) -> Expected<std::string, StsError> {
        auto urlBytes = std::string(url.view());
        auto bodyBytes = std::string(body.view());
        auto resp = httpClient.CreateNotSignedRequest()
                        .post(urlBytes, std::move(bodyBytes))
                        .headers(headers)
                        .timeout(timeoutMs)
                        .perform();
        const auto status = numericCast<int>(resp->status_code());
        if (status >= 300) {
            LOG_ERROR() << std::format("STS request failed: url={}, status={}", urlBytes, status);
            return std::unexpected(StsError::kHttpFailure);
        }
        return resp->body();
    };

    return detail::fetchStsWithExecutor(
        exec, stsEndpoint, staticAccessKeyId, staticSecretAccessKey, region, roleArn,
        roleSessionName, policyJson, duration, timeout
    );
}

} // namespace v1
