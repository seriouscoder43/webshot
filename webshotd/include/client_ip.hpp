#pragma once

#include "config.hpp"
#include "invariant.hpp"
#include "ip.hpp"
#include "text.hpp"
#include "try.hpp"
#include "userver_namespaces.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <absl/strings/strip.h>
#include <userver/server/http/http_request.hpp>
#include <userver/utils/assert.hpp>

namespace v1::client::ip {
using text::literals::operator""_t;

[[nodiscard]] inline std::optional<String> makeClientIp(std::string_view raw)
{
    std::string text{raw};
    absl::StripAsciiWhitespace(&text);
    auto ipText = TRY(String::fromBytes(text));
    auto ip = TRY(parseIp(ipText));
    return toCanonicalIpText(ip);
}

[[nodiscard]] inline std::optional<String>
resolve(const server::http::HttpRequest &request, const Config &config)
{
    switch (config.clientIpSource()) {
    case ClientIpSource::kPeer:
        return makeClientIp(request.GetRemoteAddress().PrimaryAddressString());
    case ClientIpSource::kTrustedHeader:
        return makeClientIp(request.GetHeader(config.clientIpHeaderName()));
    default:
        invariant("unknown client IP source"_t);
    }
}
} // namespace v1::client::ip
