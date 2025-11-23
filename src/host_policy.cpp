#include "host_policy.hpp"
/**
 * @file
 * @brief Host policy checks and DNS resolution for public addresses.
 */
#include "ip_utils.hpp"

#include <array>
#include <exception>
#include <string>
#include <string_view>

#include <arpa/inet.h>

#include <userver/engine/io/sockaddr.hpp>

namespace us = userver;
namespace engine = us::engine;
namespace v1::HostPolicy {

bool IsBareName(const std::string &host) { return host.find('.') == std::string::npos; }

bool IsDeniedHostname(const std::string &host)
{
    return (host == "localhost" || host == "host.docker.internal");
}

bool HasSpecialTldSuffix(std::string_view host)
{
    static const std::array<std::string_view, 5> kTlds{
        ".local", ".home.arpa", ".test", ".invalid", ".example"
    };

    for (const auto tldWithDot : kTlds) {
        const auto tldSize = tldWithDot.size();
        if (host.size() >= tldSize) {
            if (host.compare(host.size() - tldSize, tldSize, tldWithDot) == 0)
                return true;
        }

        const std::string_view plainTld = tldWithDot.substr(1);
        if (host == plainTld)
            return true;
    }
    return false;
}

std::vector<std::string> resolvePublic(
    us::clients::dns::Resolver &resolver, const std::string &host, engine::Deadline deadline
)
{
    std::vector<std::string> v4;
    try {
        auto addrs = resolver.Resolve(host, deadline);
        for (const auto &sa : addrs) {
            switch (sa.Domain()) {
            case userver::engine::io::AddrDomain::kInet: {
                const auto *sin = sa.As<struct sockaddr_in>();
                if (v4.size() < 5 && IpUtils::isPublicIpv4(sin->sin_addr))
                    v4.emplace_back(sa.PrimaryAddressString());
                break;
            }
            default:
                break;
            }
            if (v4.size() >= 5)
                break;
        }
    } catch (std::exception &) {
        // return empty to signal failure
    }
    return v4;
}

} // namespace v1::HostPolicy
