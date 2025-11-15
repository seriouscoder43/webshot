#pragma once

#include <string_view>

#include <netinet/in.h>

namespace IpUtils {

// Returns true if the hostname is an IP literal:
//  - Bracketed IPv6 literal: "[2001:db8::1]"
//  - IPv4 dotted-decimal literal: "203.0.113.5"
bool isIpLiteralHostname(std::string_view hostname) noexcept;

// Returns true if IPv4 address (host byte order) is publicly routable
// (i.e., not private/bogon/multicast/reserved ranges we disallow).
bool isPublicRoutableIPv4(uint32_t ipHostOrder) noexcept;

// Returns true if IPv6 address is publicly routable by our allowlist rules.
bool isPublicRoutableIPv6(const in6_addr &addr) noexcept;

} // namespace IpUtils
