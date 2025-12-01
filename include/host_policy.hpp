#pragma once

#include "text.hpp"

#include <userver/clients/dns/resolver.hpp>
#include <userver/engine/deadline.hpp>

namespace us = userver;

namespace v1::HostPolicy {

/** @return true if `host` has no dots. */
bool isBareName(const String &host);
/** @return true for names explicitly blocked regardless of resolution. */
bool isDeniedHostname(const String &host);
/** @return true if the name ends with a reserved/special TLD like `.local`. */
bool hasSpecialTldSuffix(String host);

/**
 * @brief Resolve a hostname and return public IPv4 addresses.
 *
 * Performs DNS resolution and filters out private, link‑local, multicast and
 * other non‑public ranges.
 *
 * @param resolver userver DNS resolver.
 * @param host Host to resolve.
 * @param deadline Resolution deadline.
 * @return List of public addresses (string form). Empty on failure.
 */
std::vector<String> resolvePublic(
    us::clients::dns::Resolver &resolver, const String &host, us::engine::Deadline deadline
);

} // namespace v1::HostPolicy
