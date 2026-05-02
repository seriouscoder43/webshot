#pragma once

#include "config.hpp"
#include "invariant.hpp"
#include "ip.hpp"
#include "text.hpp"
#include "try.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <absl/strings/strip.h>
#include <userver/server/http/http_request.hpp>
#include <userver/utils/assert.hpp>

namespace v1::client::ip {
namespace us = userver;
namespace server = us::server;
using text::literals::operator""_t;

[[nodiscard]] inline std::optional<String> MakeClientIp(std::string_view raw)
{
    std::string text{raw};
    absl::StripAsciiWhitespace(&text);
    auto ip_text = TRY(String::FromBytes(text));
    auto ip = TRY(ParseIp(ip_text));
    return ToCanonicalIpText(ip);
}

[[nodiscard]] inline std::optional<String>
Resolve(const server::http::HttpRequest &request, const Config &config)
{
    switch (config.ClientIpSource()) {
    case ClientIpSource::kPeer:
        return MakeClientIp(request.GetRemoteAddress().PrimaryAddressString());
    case ClientIpSource::kTrustedHeader:
        return MakeClientIp(request.GetHeader(config.ClientIpHeaderName()));
    default:
        Invariant("unknown client IP source"_t);
    }
}
} // namespace v1::client::ip
