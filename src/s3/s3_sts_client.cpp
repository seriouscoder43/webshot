#include "s3/s3_sts_client.hpp"

#include "link.hpp"
#include "s3/s3_url_utils.hpp"
#include "s3/sigv4_signer.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <userver/clients/http/client.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/datetime/from_string_saturating.hpp>

namespace http = userver::clients::http;

namespace v1 {

namespace {

[[nodiscard]] std::string extractXmlTag(const std::string &xml, std::string_view tag)
{
    std::string open = "<";
    open.append(tag.data(), tag.size()).push_back('>');
    std::string close = "</";
    close.append(tag.data(), tag.size()).push_back('>');
    const auto startPos = xml.find(open);
    if (startPos == std::string::npos)
        throw std::runtime_error("STS XML missing tag");
    const auto valuePos = startPos + open.size();
    const auto endPos = xml.find(close, valuePos);
    if (endPos == std::string::npos)
        throw std::runtime_error("STS XML missing closing tag");
    return xml.substr(valuePos, endPos - valuePos);
}

} // namespace

StsCredentials::StsCredentials(const std::string &xml)
    : accessKeyId(s3v4::AccessKeyId{extractXmlTag(xml, "AccessKeyId")}),
      secretAccessKey(s3v4::SecretAccessKey{extractXmlTag(xml, "SecretAccessKey")}),
      sessionToken(s3v4::SessionToken{extractXmlTag(xml, "SessionToken")}),
      expiresAt(
          userver::utils::datetime::FromRfc3339StringSaturating(extractXmlTag(xml, "Expiration"))
      )
{
}

StsCredentials detail::fetchStsWithExecutor(
    const StsExecutor &exec, const std::string &stsEndpoint,
    const s3v4::AccessKeyId &staticAccessKeyId, const s3v4::SecretAccessKey &staticSecretAccessKey,
    const std::string &region, const std::string &roleArn, const std::string &roleSessionName,
    const std::string &policyJson, std::chrono::seconds duration, std::chrono::milliseconds timeout
)
{
    const auto stsLink = Link::fromUserInput(stsEndpoint, static_cast<size_t>(stsEndpoint.size()));
    UINVARIANT(stsLink.url.type == ada::scheme::type::HTTPS, "STS endpoint must use https scheme");

    const std::string host = std::string(stsLink.url.get_host());

    std::string path = std::string(stsLink.url.get_pathname());
    if (path.empty())
        path = "/";

    std::vector<std::pair<std::string, std::string>> query;
    if (stsLink.url.has_search()) {
        const std::string search = std::string(stsLink.url.get_search());
        query = s3v4::decodeQueryString(search);
    }

    const std::string url = std::string(stsLink.url.get_href());

    std::string body;
    body.reserve(512);
    auto appendParam = [&body](std::string_view name, std::string_view value) {
        if (!body.empty())
            body.push_back('&');
        body.append(name);
        body.push_back('=');
        body.append(s3v4::percentEncode(value, true));
    };
    appendParam("Action", "AssumeRole");
    appendParam("Version", "2011-06-15");
    appendParam("RoleArn", roleArn);
    appendParam("RoleSessionName", roleSessionName);
    appendParam("DurationSeconds", std::to_string(duration.count()));
    appendParam("Policy", policyJson);

    const std::string payloadHash = s3v4::sha256Hex(body);

    const auto now = userver::utils::datetime::Now();
    s3v4::SigV4Params params(
        region, "sts", staticAccessKeyId, staticSecretAccessKey, std::nullopt, now
    );

    std::vector<std::pair<std::string, std::string>> headersToSign;
    headersToSign.emplace_back("host", host);
    headersToSign.emplace_back("content-type", "application/x-www-form-urlencoded");

    const auto signedHeaders = s3v4::signHeaders(
        params, "POST", path, query, headersToSign, payloadHash
    );

    http::Headers headers;
    headers[userver::http::headers::kHost] = host;
    headers[userver::http::headers::kContentType] = "application/x-www-form-urlencoded";
    for (const auto &kv : signedHeaders)
        headers[kv.first] = kv.second;

    const std::string xml = exec(url, body, headers, timeout);
    return StsCredentials{xml};
}

StsCredentials fetchStsCredentials(
    http::Client &httpClient, const std::string &stsEndpoint,
    const s3v4::AccessKeyId &staticAccessKeyId, const s3v4::SecretAccessKey &staticSecretAccessKey,
    const std::string &region, const std::string &roleArn, const std::string &roleSessionName,
    const std::string &policyJson, std::chrono::seconds duration, std::chrono::milliseconds timeout
)
{
    detail::StsExecutor exec = [&httpClient](
                                   const std::string &url, const std::string &body,
                                   const http::Headers &headers, std::chrono::milliseconds timeoutMs
                               ) {
        auto resp = httpClient.CreateNotSignedRequest()
                        .post(url, body)
                        .headers(headers)
                        .timeout(timeoutMs)
                        .perform();
        resp->raise_for_status();
        return resp->body();
    };

    return detail::fetchStsWithExecutor(
        exec, stsEndpoint, staticAccessKeyId, staticSecretAccessKey, region, roleArn,
        roleSessionName, policyJson, duration, timeout
    );
}

} // namespace v1
