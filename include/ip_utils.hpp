#pragma once

#include <netinet/in.h>

namespace IpUtils {

/** @brief IPv4 address (network byte order) passes public‑routable allowlist. */
[[nodiscard]] bool isPublicIpv4(const in_addr &addr) noexcept;

/** @brief IPv6 address passes public‑routable allowlist rules. */
[[nodiscard]] bool isPublicIpv6(const in6_addr &addr) noexcept;

} // namespace IpUtils
