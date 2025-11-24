#include "s3/sigv4_signer.hpp"
/**
 * @file
 * @brief Helpers for AWS Signature V4 request canonicalization and signing.
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>

#include <cctz/time_zone.h>

#include <userver/crypto/hash.hpp>

#include <absl/strings/ascii.h>
#include <fmt/format.h>

namespace v1::s3v4 {

namespace {

/** Characters that do not require percent‑encoding. */
inline bool IsUnreserved(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '_' || c == '.' || c == '~';
}

/** Collapse runs of spaces and trim at both ends. */
std::string TrimSpaces(const std::string &s)
{
    absl::string_view trimmed = absl::StripAsciiWhitespace(absl::string_view{s});
    std::string out;
    out.reserve(trimmed.size());
    bool in_space = false;
    for (char c : trimmed) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                out.push_back(' ');
                in_space = true;
            }
        } else {
            out.push_back(c);
            in_space = false;
        }
    }
    return out;
}

/** Join header names with semicolons in order. */
std::string JoinSignedHeaders(const std::vector<std::pair<std::string, std::string>> &headers)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < headers.size(); i++) {
        if (i)
            oss << ';';
        oss << headers[i].first;
    }
    return oss.str();
}

std::string canonicalizeQueryImpl(const std::vector<std::pair<std::string, std::string>> &query)
{
    std::vector<std::pair<std::string, std::string>> q = query;
    for (auto &kv : q) {
        kv.first = percentEncode(kv.first, /*encodeSlash*/ true);
        kv.second = percentEncode(kv.second, /*encodeSlash*/ true);
    }
    std::sort(q.begin(), q.end(), [](const auto &a, const auto &b) {
        if (a.first == b.first)
            return a.second < b.second;
        return a.first < b.first;
    });
    std::ostringstream canon_query;
    for (size_t i = 0; i < q.size(); i++) {
        if (i)
            canon_query << '&';
        canon_query << q[i].first << '=' << q[i].second;
    }
    return canon_query.str();
}

} // namespace

SigV4Params::SigV4Params(
    std::string region_, std::string service_, const AccessKeyId &accessKeyId_,
    const SecretAccessKey &secretAccessKey_, std::optional<SessionToken> sessionToken_,
    const std::chrono::system_clock::time_point &now
)
    : region(std::move(region_)), service(std::move(service_)), accessKeyId(accessKeyId_),
      secretAccessKey(secretAccessKey_), sessionToken(std::move(sessionToken_)),
      amzDate(toAmzDateUtc(now)), date(toDateStampUtc(now))
{
}

std::string buildScope(const SigV4Params &params)
{
    return fmt::format("{}/{}/{}/aws4_request", params.date, params.region, params.service);
}

std::string computeSignature(const SigV4Params &params, std::string_view string_to_sign)
{
    namespace US = USERVER_NAMESPACE::crypto::hash;
    const std::string kSecret = fmt::format("AWS4{}", params.secretAccessKey.GetUnderlying());
    const std::string kDate = US::HmacSha256(kSecret, params.date, US::OutputEncoding::kBinary);
    const std::string kRegion = US::HmacSha256(kDate, params.region, US::OutputEncoding::kBinary);
    const std::string kService = US::HmacSha256(
        kRegion, params.service, US::OutputEncoding::kBinary
    );
    const std::string kSigning = US::HmacSha256(
        kService, "aws4_request", US::OutputEncoding::kBinary
    );
    return US::HmacSha256(kSigning, string_to_sign, US::OutputEncoding::kHex);
}

std::string toAmzDateUtc(std::chrono::system_clock::time_point tp)
{
    const auto tz = cctz::utc_time_zone();
    return cctz::format("%Y%m%dT%H%M%SZ", tp, tz);
}

std::string toDateStampUtc(std::chrono::system_clock::time_point tp)
{
    const auto tz = cctz::utc_time_zone();
    return cctz::format("%Y%m%d", tp, tz);
}

std::string sha256Hex(std::string_view data)
{
    return USERVER_NAMESPACE::crypto::hash::Sha256(
        data, USERVER_NAMESPACE::crypto::hash::OutputEncoding::kHex
    );
}

std::string percentEncode(std::string_view s, bool encodeSlash)
{
    std::string out;
    out.reserve(s.size() * 3);
    for (char c : s) {
        if (IsUnreserved(c) || (!encodeSlash && c == '/')) {
            out.push_back(c);
        } else {
            unsigned char uc = static_cast<unsigned char>(c);
            static const char *kHex = "0123456789ABCDEF";
            out.push_back('%');
            out.push_back(kHex[(uc >> 4) & 0xF]);
            out.push_back(kHex[uc & 0xF]);
        }
    }
    return out;
}

CanonicalRequestParts buildCanonicalRequest(
    std::string_view method, std::string_view canonicalUri,
    const std::vector<std::pair<std::string, std::string>> &query,
    const std::vector<std::pair<std::string, std::string>> &headersLowercaseTrimmedSorted,
    std::string_view payloadSha256Hex
)
{
    // Canonical URI must be URI-encoded with slash preserved
    const std::string canon_uri = percentEncode(canonicalUri, /*encodeSlash*/ false);

    const std::string canon_query = canonicalizeQueryImpl(query);

    std::ostringstream canon_headers;
    for (const auto &kv : headersLowercaseTrimmedSorted) {
        canon_headers << kv.first << ':' << TrimSpaces(kv.second) << "\n";
    }
    std::string signed_headers = JoinSignedHeaders(headersLowercaseTrimmedSorted);

    std::ostringstream oss;
    oss << method << "\n"
        << canon_uri << "\n"
        << canon_query << "\n"
        << canon_headers.str() << "\n"
        << signed_headers << "\n"
        << payloadSha256Hex;

    return {oss.str(), signed_headers};
}

std::string canonicalizeQuery(const std::vector<std::pair<std::string, std::string>> &decoded)
{
    return canonicalizeQueryImpl(decoded);
}

std::vector<std::pair<std::string, std::string>>
prepareSignedHeaders(std::string host, const userver::clients::http::Headers &extra)
{
    std::vector<std::pair<std::string, std::string>> v;
    v.reserve(extra.size() + 1);
    v.emplace_back("host", std::move(host));
    for (const auto &kv : extra) {
        std::string keyLower;
        keyLower.reserve(kv.first.size());
        for (char c : kv.first)
            keyLower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        v.emplace_back(std::move(keyLower), kv.second);
    }
    std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    return v;
}

std::string buildSignedHeaders(
    const std::vector<std::pair<std::string, std::string>> &headersLowercaseTrimmedSorted
)
{
    return JoinSignedHeaders(headersLowercaseTrimmedSorted);
}

std::unordered_map<std::string, std::string> signHeaders(
    const SigV4Params &p, std::string_view method, std::string_view canonicalUri,
    const std::vector<std::pair<std::string, std::string>> &query,
    const std::vector<std::pair<std::string, std::string>> &headersLowerTrimmed,
    std::string_view payloadSha256Hex
)
{
    std::vector<std::pair<std::string, std::string>> headers = headersLowerTrimmed;
    // Add required headers
    std::unordered_map<std::string, std::string> out;
    out["x-amz-date"] = p.amzDate;
    out["x-amz-content-sha256"] = std::string(payloadSha256Hex);
    if (p.sessionToken)
        out["x-amz-security-token"] = p.sessionToken->GetUnderlying();
    // Merge to headers vector for canonicalization and sort
    for (const auto &kv : out)
        headers.emplace_back(kv.first, kv.second);
    std::sort(headers.begin(), headers.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });

    const auto cr = buildCanonicalRequest(method, canonicalUri, query, headers, payloadSha256Hex);

    const std::string scope = buildScope(p);
    const std::string string_to_sign = fmt::format(
        "AWS4-HMAC-SHA256\n{}\n{}\n{}", p.amzDate, scope, sha256Hex(cr.canonicalRequest)
    );

    const std::string signature = computeSignature(p, string_to_sign);

    const std::string credential = fmt::format("{}/{}", p.accessKeyId.GetUnderlying(), scope);
    std::string authorization = fmt::format(
        "AWS4-HMAC-SHA256 Credential={}, SignedHeaders={}, Signature={}", credential,
        cr.signedHeaders, signature
    );

    out["authorization"] = std::move(authorization);
    return out;
}

} // namespace v1::s3v4
