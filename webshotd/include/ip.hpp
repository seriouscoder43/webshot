#pragma once

#include "text.hpp"
#include "userver_namespaces.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <variant>

#include <arpa/inet.h>
#include <userver/utils/ip.hpp>

namespace v1 {

using Ip4 = us::utils::ip::AddressV4;
using Ip6 = us::utils::ip::AddressV6;
using Ip = std::variant<Ip4, Ip6>;

namespace ip_detail {

[[nodiscard]] inline bool inIpv4Range(uint32_t ip, uint32_t network, uint32_t mask) noexcept
{
    return (ip & mask) == (network & mask);
}

[[nodiscard]] inline uint32_t ipv4HostOrder(const Ip4 &ip) noexcept
{
    const auto &b = ip.GetBytes();
    return (uint32_t{b[0]} << 24U) | (uint32_t{b[1]} << 16U) | (uint32_t{b[2]} << 8U) |
           uint32_t{b[3]};
}

} // namespace ip_detail

[[nodiscard]] inline std::optional<Ip4> parseIp4(const String &text) noexcept
{
    if (text.empty())
        return {};

    in_addr addr4{};
    const std::string hostText{text.view()};
    if (inet_pton(AF_INET, hostText.c_str(), &addr4) != 1)
        return {};

    Ip4::BytesType bytes{};
    std::memcpy(bytes.data(), &addr4.s_addr, bytes.size());
    return Ip4{bytes};
}

[[nodiscard]] inline std::optional<Ip6> parseIp6(const String &text) noexcept
{
    if (text.empty())
        return {};

    auto candidate = text.view();
    if (candidate.front() == '[' && candidate.back() == ']')
        candidate = candidate.substr(1, candidate.size() - 2);

    in6_addr addr6{};
    const std::string hostText{candidate};
    if (inet_pton(AF_INET6, hostText.c_str(), &addr6) != 1)
        return {};

    Ip6::BytesType bytes{};
    std::memcpy(bytes.data(), addr6.s6_addr, bytes.size());
    return Ip6{bytes};
}

[[nodiscard]] inline std::optional<Ip> parseIp(const String &text) noexcept
{
    if (auto addr = parseIp4(text))
        return Ip{*addr};
    if (auto addr = parseIp6(text))
        return Ip{*addr};
    return {};
}

[[nodiscard]] inline bool isIpLiteralHostname(const String &host) noexcept
{
    return static_cast<bool>(parseIp(host));
}

[[nodiscard]] inline bool isPublicRoutable(const Ip4 &addr) noexcept
{
    const auto ip = ip_detail::ipv4HostOrder(addr);

    if (ip_detail::inIpv4Range(ip, 0x00000000u, 0xFF000000u)) // 0.0.0.0/8
        return false;
    if (ip_detail::inIpv4Range(ip, 0x0A000000u, 0xFF000000u)) // 10.0.0.0/8
        return false;
    if (ip_detail::inIpv4Range(ip, 0x64400000u, 0xFFC00000u)) // 100.64.0.0/10
        return false;
    if (ip_detail::inIpv4Range(ip, 0x7F000000u, 0xFF000000u)) // 127.0.0.0/8
        return false;
    if (ip_detail::inIpv4Range(ip, 0xA9FE0000u, 0xFFFF0000u)) // 169.254.0.0/16
        return false;
    if (ip_detail::inIpv4Range(ip, 0xAC100000u, 0xFFF00000u)) // 172.16.0.0/12
        return false;
    if (ip_detail::inIpv4Range(ip, 0xC0A80000u, 0xFFFF0000u)) // 192.168.0.0/16
        return false;
    if (ip_detail::inIpv4Range(ip, 0xC6120000u, 0xFFFE0000u)) // 198.18.0.0/15
        return false;
    if (ip_detail::inIpv4Range(ip, 0xE0000000u, 0xF0000000u)) // 224.0.0.0/4
        return false;
    if (ip_detail::inIpv4Range(ip, 0xF0000000u, 0xF0000000u)) // 240.0.0.0/4
        return false;
    return true;
}

[[nodiscard]] inline bool isPublicRoutable(const Ip6 &addr) noexcept
{
    const auto &v = addr.GetBytes();

    if (std::ranges::all_of(v, [](unsigned char c) { return c == 0; })) // ::/128
        return false;

    constexpr std::array<unsigned char, 16> kLoopback{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    };
    if (v == kLoopback) // ::1/128
        return false;

    if ((v[0] & 0xFEu) == 0xFCu) // fc00::/7
        return false;
    if (v[0] == 0xFEu && (v[1] & 0xC0u) == 0x80u) // fe80::/10
        return false;

    constexpr std::array<unsigned char, 10> kIpv4MappedPrefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (std::equal(std::begin(kIpv4MappedPrefix), std::end(kIpv4MappedPrefix), std::begin(v)) &&
        v[10] == 0xFFu && v[11] == 0xFFu) {
        return false;
    }

    if (v[0] == 0xFFu) // ff00::/8
        return false;
    return true;
}

[[nodiscard]] inline bool isPublicRoutable(const Ip &addr) noexcept
{
    return std::visit([](const auto &typed) { return isPublicRoutable(typed); }, addr);
}

} // namespace v1
