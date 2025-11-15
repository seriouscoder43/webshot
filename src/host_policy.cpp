#include "include/host_policy.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <string>

using namespace std::chrono_literals;
namespace us = userver;
namespace engine = us::engine;
namespace v1::hostpolicy {

bool IsBareName(const std::string &host_lower) { return host_lower.find('.') == std::string::npos; }

bool IsDeniedHostname(const std::string &host_lower)
{
    return (host_lower == "localhost" || host_lower == "host.docker.internal");
}

bool HasSpecialTldSuffix(const std::string &host_lower)
{
    static const std::array<const char *, 5> kTlds{
        ".local", ".home.arpa", ".test", ".invalid", ".example"
    };
    for (auto *tld : kTlds) {
        if (host_lower.size() >= std::strlen(tld)) {
            if (host_lower.compare(host_lower.size() - std::strlen(tld), std::strlen(tld), tld) ==
                0)
                return true;
        }
        if (host_lower == tld + 1)
            return true; // plain tld (e.g., "local") — unlikely after bare-name check
    }
    return false;
}

static bool IsPublicV4(uint32_t ip_be)
{
    uint32_t ip = ::ntohl(ip_be);
    auto inr = [&](uint32_t net, uint32_t mask) { return (ip & mask) == (net & mask); };
    if (inr(0x00000000u, 0xFF000000u))
        return false; // 0.0.0.0/8
    if (inr(0x0A000000u, 0xFF000000u))
        return false; // 10.0.0.0/8
    if (inr(0x64400000u, 0xFFC00000u))
        return false; // 100.64.0.0/10
    if (inr(0x7F000000u, 0xFF000000u))
        return false; // 127.0.0.0/8
    if (inr(0xA9FE0000u, 0xFFFF0000u))
        return false; // 169.254.0.0/16
    if (inr(0xAC100000u, 0xFFF00000u))
        return false; // 172.16.0.0/12
    if (inr(0xC0A80000u, 0xFFFF0000u))
        return false; // 192.168.0.0/16
    if (inr(0xE0000000u, 0xF0000000u))
        return false; // 224.0.0.0/4
    if (inr(0xF0000000u, 0xF0000000u))
        return false; // 240.0.0.0/4
    return true;
}

static bool IsPublicV6(const struct in6_addr &a)
{
    const unsigned char *v = a.s6_addr;
    auto all_zero = std::all_of(v, v + 16, [](unsigned char c) { return c == 0; });
    if (all_zero)
        return false;     // ::/128
    bool loopback = true; // ::1/128
    for (int i = 0; i < 15; ++i)
        loopback &= (v[i] == 0);
    loopback &= (v[15] == 1);
    if (loopback)
        return false;
    if ((v[0] & 0xFE) == 0xFC)
        return false; // fc00::/7 ULA
    if (v[0] == 0xFE && (v[1] & 0xC0) == 0x80)
        return false; // fe80::/10 link-local
    if (v[0] == 0xFF)
        return false; // ff00::/8 multicast
    // ::ffff:0:0/96 mapped
    bool mapped = true;
    for (int i = 0; i < 10; ++i)
        mapped &= (v[i] == 0);
    mapped &= (v[10] == 0xFF && v[11] == 0xFF);
    if (mapped)
        return false;
    return true;
}

std::vector<std::string> resolvePublic(
    us::clients::dns::Resolver &resolver, const std::string &host, std::chrono::milliseconds timeout
)
{
    std::vector<std::string> out;
    try {
        auto addrs = resolver.Resolve(host, engine::Deadline::FromDuration(timeout));
        for (const auto &sa : addrs) {
            switch (sa.Domain()) {
            case userver::engine::io::AddrDomain::kInet: {
                const auto *sin = sa.As<struct sockaddr_in>();
                if (IsPublicV4(sin->sin_addr.s_addr))
                    out.emplace_back(sa.PrimaryAddressString());
                break;
            }
            case userver::engine::io::AddrDomain::kInet6: {
                const auto *sin6 = sa.As<struct sockaddr_in6>();
                if (IsPublicV6(sin6->sin6_addr))
                    out.emplace_back(sa.PrimaryAddressString());
                break;
            }
            default:
                break;
            }
            if (out.size() >= 5)
                break;
        }
    } catch (std::exception &) {
        // swallow, return empty to signal failure
    }
    return out;
}

} // namespace v1::hostpolicy
