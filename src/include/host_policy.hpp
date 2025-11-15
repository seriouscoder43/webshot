#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <userver/clients/dns/resolver.hpp>

namespace us = userver;

namespace v1::hostpolicy {

bool IsBareName(const std::string &host_lower);
bool IsDeniedHostname(const std::string &host_lower);
bool HasSpecialTldSuffix(const std::string &host_lower);

// Resolve and return public addresses as strings (IPv4/IPv6), filtered from private/bogon ranges
std::vector<std::string> resolvePublic(
    us::clients::dns::Resolver &resolver, const std::string &host, std::chrono::milliseconds timeout
);

} // namespace v1::hostpolicy
