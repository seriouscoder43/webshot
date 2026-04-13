#include "crawler/egress_proxy.hpp"
/**
 * @file
 * @brief In-process forward proxy used by the sandboxed Chromium crawler.
 *
 * Implements a per-run Unix-socket HTTP proxy (CONNECT + plain HTTP) that
 * bridges the network namespace boundary and counts downstream bytes
 * (proxy -> browser). This proxy does not perform TLS MITM.
 */

#include "integers.hpp"
#include "ip_utils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <userver/clients/dns/exception.hpp>
#include <userver/clients/dns/resolver.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/crypto/exception.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/engine/io/socket.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/engine/wait_any.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/traceful_exception.hpp>

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/strip.h>

namespace v1::crawler {
using namespace text::literals;
namespace concurrent = us::concurrent;
namespace crypto = us::crypto;
namespace dns = us::clients::dns;
namespace engine = eng;
namespace utils = us::utils;

namespace {

constexpr size_t kMaxHeaderBytes = 64UL * 1024UL;
constexpr size_t kIoBufferBytes = 16UL * 1024UL;

constexpr std::string_view kLocalHostA = "test-target";
constexpr std::string_view kLocalHostB = "asset.test-target";
constexpr auto kLocalHttpPort = 18080_u16;
constexpr auto kLocalHttpsPort = 18443_u16;

struct [[nodiscard]] HeaderLine final {
    std::string nameLower;
    std::string nameOriginal;
    std::string value;
};

[[nodiscard]] std::optional<std::string_view>
findHeaderValue(const std::vector<HeaderLine> &headers, std::string_view nameLower) noexcept
{
    for (const auto &h : headers) {
        if (h.nameLower == nameLower)
            return std::string_view(h.value);
    }
    return {};
}

struct [[nodiscard]] ParsedRequest final {
    std::string method;
    std::string target;
    std::string version;
    std::vector<HeaderLine> headers;
    size_t headerBytes;
};

[[nodiscard]] std::optional<ParsedRequest> parseHeaderBlock(std::string_view bytes)
{
    const auto endPos = bytes.find("\r\n\r\n");
    if (endPos == std::string_view::npos)
        return {};

    const auto headerPart = bytes.substr(0, endPos + 4);
    size_t pos = 0;
    const auto lineEnd = headerPart.find("\r\n", pos);
    if (lineEnd == std::string_view::npos)
        return {};
    const auto requestLine = headerPart.substr(0, lineEnd);
    pos = lineEnd + 2;

    ParsedRequest out;
    out.headerBytes = headerPart.size();
    {
        const auto firstSp = requestLine.find(' ');
        if (firstSp == std::string_view::npos)
            return {};
        const auto secondSp = requestLine.find(' ', firstSp + 1);
        if (secondSp == std::string_view::npos)
            return {};
        out.method = std::string(requestLine.substr(0, firstSp));
        out.target = std::string(requestLine.substr(firstSp + 1, secondSp - firstSp - 1));
        out.version = std::string(requestLine.substr(secondSp + 1));
        if (out.method.empty() || out.target.empty() || out.version.empty())
            return {};
    }

    while (pos < headerPart.size()) {
        const auto next = headerPart.find("\r\n", pos);
        if (next == std::string_view::npos)
            return {};
        if (next == pos) {
            pos += 2;
            break;
        }
        const auto line = headerPart.substr(pos, next - pos);
        pos = next + 2;

        const auto colon = line.find(':');
        if (colon == std::string_view::npos)
            continue;
        auto name = line.substr(0, colon);
        auto value = line.substr(colon + 1);
        value = absl::StripAsciiWhitespace(value);
        if (name.empty())
            continue;

        HeaderLine h;
        h.nameOriginal = std::string(name);
        h.nameLower = absl::AsciiStrToLower(name);
        h.value = std::string(value);
        out.headers.push_back(std::move(h));
    }
    return out;
}

[[nodiscard]] std::optional<std::pair<std::string, std::string>>
parseBasicAuthUser(std::string_view headerValue)
{
    auto value = headerValue;
    const auto sp = value.find(' ');
    if (sp == std::string_view::npos)
        return {};
    const auto scheme = value.substr(0, sp);
    if (!absl::EqualsIgnoreCase(scheme, "basic"))
        return {};
    value.remove_prefix(sp + 1);
    value = absl::StripLeadingAsciiWhitespace(value);
    if (value.empty())
        return {};

    try {
        auto decoded = crypto::base64::Base64Decode(std::string(value));
        auto pos = decoded.find(':');
        if (pos == std::string::npos)
            return {};
        auto username = decoded.substr(0, pos);
        auto password = decoded.substr(pos + 1);
        return std::pair{std::move(username), std::move(password)};
    } catch (const crypto::CryptoException &) {
        return {};
    }
}

[[nodiscard]] std::optional<std::pair<std::string, u16>>
parseAuthorityHostPort(std::string_view authority)
{
    auto host = std::string_view{};
    auto portText = std::string_view{};

    if (authority.empty())
        return {};
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos)
            return {};
        host = authority.substr(0, close + 1);
        if (close + 1 >= authority.size() || authority[close + 1] != ':')
            return {};
        portText = authority.substr(close + 2);
    } else {
        const auto colon = authority.rfind(':');
        if (colon == std::string_view::npos)
            return {};
        host = authority.substr(0, colon);
        portText = authority.substr(colon + 1);
    }

    if (host.empty() || portText.empty())
        return {};
    auto port = 0_i64;
    for (char ch : portText) {
        if (ch < '0' || ch > '9')
            return {};
        port = port * 10_i64 + i64(ch - '0');
        if (port > i64(65535_u16))
            return {};
    }
    if (port <= 0_i64)
        return {};
    return std::pair{std::string(host), u16(port)};
}

[[nodiscard]] std::optional<std::pair<std::string, u16>>
parseHostHeaderAuthority(std::string_view hostHeader)
{
    auto trimmed = absl::StripAsciiWhitespace(hostHeader);
    if (trimmed.empty())
        return {};
    if (trimmed.find(':') == std::string_view::npos)
        return std::pair{std::string(trimmed), 0_u16};
    return parseAuthorityHostPort(trimmed);
}

[[nodiscard]] engine::io::Sockaddr sockaddrFromIpv4(std::string_view host, u16 port)
{
    sockaddr_in addr4{};
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(raw(port));
    const auto hostText = std::string(host);
    UINVARIANT(inet_pton(AF_INET, hostText.c_str(), &addr4.sin_addr) == 1, "invalid ipv4 addr");
    return engine::io::Sockaddr(&addr4);
}

[[nodiscard]] engine::io::Sockaddr sockaddrFromIpv6(std::string_view host, u16 port)
{
    auto candidate = host;
    if (!candidate.empty() && candidate.front() == '[' && candidate.back() == ']')
        candidate = candidate.substr(1, candidate.size() - 2);
    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(raw(port));
    const auto hostText = std::string(candidate);
    UINVARIANT(inet_pton(AF_INET6, hostText.c_str(), &addr6.sin6_addr) == 1, "invalid ipv6 addr");
    return engine::io::Sockaddr(&addr6);
}

[[nodiscard]] Expected<engine::io::Sockaddr, String>
resolveTcp(dns::Resolver &resolver, const std::string &host, u16 port, engine::Deadline deadline)
{
    if (isIpv4Address(host))
        return sockaddrFromIpv4(host, port);
    if (isIpv6Address(host))
        return sockaddrFromIpv6(host, port);

    try {
        auto addrs = resolver.Resolve(host, deadline);
        if (addrs.empty())
            return std::unexpected(text::format("dns resolve returned no addresses for {}", host));
        addrs.front().SetPort(raw(port));
        return addrs.front();
    } catch (const dns::NotResolvedException &) {
        return std::unexpected(text::format("dns resolve failed for {}", host));
    }
}

struct [[nodiscard]] UpstreamTarget final {
    std::string connectHost;
    u16 connectPort;
};

[[nodiscard]] UpstreamTarget
rewriteLocalFixtureIfNeeded(const EgressProxyConfig &cfg, std::string host, u16 port)
{
    if (!cfg.enableLocalFixtureRewrite)
        return UpstreamTarget{.connectHost = std::move(host), .connectPort = port};

    const bool isLocalHost = host == kLocalHostA || host == kLocalHostB;
    if (!isLocalHost)
        return UpstreamTarget{.connectHost = std::move(host), .connectPort = port};

    if (port == 80_u16)
        return UpstreamTarget{.connectHost = "127.0.0.1", .connectPort = kLocalHttpPort};
    if (port == 443_u16)
        return UpstreamTarget{.connectHost = "127.0.0.1", .connectPort = kLocalHttpsPort};
    return UpstreamTarget{.connectHost = std::move(host), .connectPort = port};
}

[[nodiscard]] std::string_view stripBrackets(std::string_view host) noexcept
{
    if (absl::StartsWith(host, "[") && absl::EndsWith(host, "]") && host.size() >= 2)
        return host.substr(1, host.size() - 2);
    return host;
}

[[nodiscard]] std::string make407Response()
{
    return std::string(
        "HTTP/1.1 407 Proxy Authentication Required\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Proxy-Authenticate: Basic realm=\"webshot\"\r\n"
        "Connection: close\r\n"
        "Content-Length: 30\r\n"
        "\r\n"
        "Proxy authentication required\n"
    );
}

[[nodiscard]] std::string make400Response(std::string_view message)
{
    const auto body = std::string(message) + "\n";
    return std::format(
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: {}\r\n"
        "\r\n"
        "{}",
        body.size(), body
    );
}

[[nodiscard]] std::string make502Response(std::string_view message)
{
    const auto body = std::string(message) + "\n";
    return std::format(
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: {}\r\n"
        "\r\n"
        "{}",
        body.size(), body
    );
}

} // namespace

struct EgressProxy::Impl final {
    explicit Impl(EgressProxyConfig cfg) : config(std::move(cfg)) {}

    EgressProxyConfig config;
    std::atomic<int64_t> downBytes{};
    std::atomic<bool> closed{false};
    concurrent::Variable<std::optional<String>> failure;
    engine::io::Socket listener;
    std::optional<engine::TaskWithResult<void>> acceptTask;
    concurrent::Variable<std::vector<engine::TaskWithResult<void>>> clientTasks;

    [[nodiscard]] bool isClosed() const noexcept { return closed.load(std::memory_order_relaxed); }

    void noteFailure(String reason) noexcept
    {
        auto lock = failure.Lock();
        if (*lock)
            return;
        *lock = std::move(reason);
    }

    void requestCancelAllClientTasksNoWait() noexcept
    {
        if (acceptTask)
            acceptTask->RequestCancel();
        auto lock = clientTasks.Lock();
        for (auto &task : *lock)
            task.RequestCancel();
    }

    void accountDownBytes(i64 bytes) noexcept
    {
        if (bytes <= 0_i64)
            return;
        const auto delta = raw(bytes);
        const auto next = downBytes.fetch_add(delta, std::memory_order_relaxed) + delta;
        if (next <= raw(config.downBytesMax))
            return;
        noteFailure(
            text::format(
                "net_limit: proxy downstream bytes {} exceeded limit {}", next, config.downBytesMax
            )
        );
        closed.store(true, std::memory_order_relaxed);
        requestCancelAllClientTasksNoWait();
    }

    [[nodiscard]] size_t sendBudgeted(
        engine::io::Socket &sock, const void *data, size_t len, engine::Deadline deadline
    ) noexcept
    {
        if (isClosed())
            return 0;
        const auto used = i64(downBytes.load(std::memory_order_relaxed));
        const auto remaining = config.downBytesMax - used;
        if (remaining <= 0_i64) {
            noteFailure(
                text::format(
                    "net_limit: proxy downstream bytes {} exceeded limit {}", used,
                    config.downBytesMax
                )
            );
            closed.store(true, std::memory_order_relaxed);
            return 0;
        }

        const auto allowed = std::min(remaining, i64(len));
        try {
            const auto sent = sock.SendAll(data, numericCast<size_t>(allowed), deadline);
            accountDownBytes(i64(sent));
            return sent;
        } catch (const utils::TracefulException &) {
            return 0;
        }
    }

    void closeSocketsQuietly(engine::io::Socket &a, engine::io::Socket &b) noexcept
    {
        try {
            a.Close();
        } catch (const utils::TracefulException &) {
        }
        try {
            b.Close();
        } catch (const utils::TracefulException &) {
        }
    }

    void handleConnect(
        dns::Resolver &resolver, engine::io::Socket &client, const ParsedRequest &req,
        engine::Deadline deadline
    ) noexcept
    {
        const auto authority = parseAuthorityHostPort(req.target);
        if (!authority) {
            const auto resp = make400Response("invalid CONNECT target");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }
        auto [rawHost, port] = authority.value();
        const auto host = std::string(stripBrackets(rawHost));

        const auto upstream = rewriteLocalFixtureIfNeeded(config, host, port);
        auto addr = resolveTcp(resolver, upstream.connectHost, upstream.connectPort, deadline);
        if (!addr) {
            const auto resp = make502Response(addr.error().view());
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        engine::io::Socket upstreamSocket{addr->Domain(), engine::io::SocketType::kStream};
        try {
            upstreamSocket.Connect(addr.value(), deadline);
        } catch (const utils::TracefulException &) {
            const auto resp = make502Response("connect upstream failed");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        const auto ok = std::string("HTTP/1.1 200 Connection Established\r\n\r\n");
        static_cast<void>(sendBudgeted(client, ok.data(), ok.size(), deadline));
        if (isClosed()) {
            closeSocketsQuietly(client, upstreamSocket);
            return;
        }

        auto clientToUpstream = engine::AsyncNoSpan([&]() noexcept {
            std::array<char, kIoBufferBytes> buf{};
            try {
                while (!isClosed()) {
                    const auto n = client.RecvSome(buf.data(), buf.size(), deadline);
                    if (n == 0)
                        return;
                    static_cast<void>(upstreamSocket.SendAll(buf.data(), n, deadline));
                }
            } catch (const utils::TracefulException &) {
                return;
            }
        });
        auto upstreamToClient = engine::AsyncNoSpan([&]() noexcept {
            std::array<char, kIoBufferBytes> buf{};
            try {
                while (!isClosed()) {
                    const auto n = upstreamSocket.RecvSome(buf.data(), buf.size(), deadline);
                    if (n == 0)
                        return;
                    size_t pos = 0;
                    while (pos < n && !isClosed()) {
                        const auto sent = sendBudgeted(client, buf.data() + pos, n - pos, deadline);
                        if (sent == 0)
                            return;
                        pos += sent;
                    }
                }
            } catch (const utils::TracefulException &) {
                return;
            }
        });

        static_cast<void>(engine::WaitAnyUntil(deadline, clientToUpstream, upstreamToClient));
        clientToUpstream.RequestCancel();
        upstreamToClient.RequestCancel();
        static_cast<void>(clientToUpstream.WaitNothrow());
        static_cast<void>(upstreamToClient.WaitNothrow());
        closeSocketsQuietly(client, upstreamSocket);
    }

    void handleHttp(
        dns::Resolver &resolver, engine::io::Socket &client, const ParsedRequest &req,
        std::string_view headerBytes, engine::Deadline deadline
    ) noexcept
    {
        const auto hostHeader = findHeaderValue(req.headers, "host");
        std::string host;
        auto port = 80_u16;

        std::string path{"/"};
        if (absl::StartsWith(req.target, "http://")) {
            auto rest = std::string_view(req.target);
            rest.remove_prefix(std::string_view("http://").size());
            const auto slash = rest.find('/');
            const auto authority = slash == std::string_view::npos ? rest : rest.substr(0, slash);
            const auto parsedAuthority = parseAuthorityHostPort(authority);
            if (parsedAuthority) {
                host = std::string(stripBrackets(parsedAuthority->first));
                port = parsedAuthority->second;
            } else {
                host = std::string(authority);
            }
            path = slash == std::string_view::npos ? "/" : std::string(rest.substr(slash));
        } else if (absl::StartsWith(req.target, "/")) {
            if (!hostHeader) {
                const auto resp = make400Response("missing Host header");
                static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
                return;
            }
            const auto parsedHost = parseHostHeaderAuthority(hostHeader.value());
            if (!parsedHost) {
                const auto resp = make400Response("invalid Host header");
                static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
                return;
            }
            host = std::string(stripBrackets(parsedHost->first));
            if (parsedHost->second != 0_u16)
                port = parsedHost->second;
            path = req.target;
        } else {
            const auto resp = make400Response("unsupported request target");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        auto upstream = rewriteLocalFixtureIfNeeded(config, host, port);
        auto addr = resolveTcp(resolver, upstream.connectHost, upstream.connectPort, deadline);
        if (!addr) {
            const auto resp = make502Response(addr.error().view());
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }
        engine::io::Socket upstreamSocket{addr->Domain(), engine::io::SocketType::kStream};
        try {
            upstreamSocket.Connect(addr.value(), deadline);
        } catch (const utils::TracefulException &) {
            const auto resp = make502Response("connect upstream failed");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        const auto contentLengthHeader = findHeaderValue(req.headers, "content-length");
        i64 contentLength = 0_i64;
        if (contentLengthHeader) {
            for (char ch : contentLengthHeader.value()) {
                if (ch < '0' || ch > '9') {
                    contentLength = 0_i64;
                    break;
                }
                contentLength = contentLength * 10_i64 + i64(ch - '0');
                if (contentLength > 1024_i64 * 1024_i64 * 1024_i64) {
                    contentLength = 0_i64;
                    break;
                }
            }
        }

        std::string out;
        out.reserve(req.headerBytes + 64);
        out += req.method;
        out.push_back(' ');
        out += path;
        out.push_back(' ');
        out += req.version;
        out += "\r\n";
        for (const auto &h : req.headers) {
            if (h.nameLower == "proxy-authorization" || h.nameLower == "proxy-connection" ||
                h.nameLower == "connection") {
                continue;
            }
            out += h.nameOriginal;
            out += ": ";
            out += h.value;
            out += "\r\n";
        }
        out += "Connection: close\r\n";
        out += "\r\n";

        try {
            static_cast<void>(upstreamSocket.SendAll(out.data(), out.size(), deadline));
        } catch (const utils::TracefulException &) {
            const auto resp = make502Response("send upstream failed");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            closeSocketsQuietly(client, upstreamSocket);
            return;
        }

        auto remainingBody = contentLength;
        auto alreadyBody = headerBytes.substr(req.headerBytes);
        if (!alreadyBody.empty()) {
            const auto take = std::min(remainingBody, i64(alreadyBody.size()));
            try {
                static_cast<void>(
                    upstreamSocket.SendAll(alreadyBody.data(), numericCast<size_t>(take), deadline)
                );
            } catch (const utils::TracefulException &) {
                closeSocketsQuietly(client, upstreamSocket);
                return;
            }
            remainingBody -= take;
        }

        std::array<char, kIoBufferBytes> buf{};
        while (remainingBody > 0_i64 && !isClosed()) {
            const auto want = std::min(remainingBody, i64(buf.size()));
            size_t n = 0;
            try {
                n = client.RecvSome(buf.data(), numericCast<size_t>(want), deadline);
            } catch (const utils::TracefulException &) {
                break;
            }
            if (n == 0)
                break;
            remainingBody -= i64(n);
            try {
                static_cast<void>(upstreamSocket.SendAll(buf.data(), n, deadline));
            } catch (const utils::TracefulException &) {
                break;
            }
        }

        while (!isClosed()) {
            size_t n = 0;
            try {
                n = upstreamSocket.RecvSome(buf.data(), buf.size(), deadline);
            } catch (const utils::TracefulException &) {
                break;
            }
            if (n == 0)
                break;
            size_t pos = 0;
            while (pos < n && !isClosed()) {
                const auto sent = sendBudgeted(client, buf.data() + pos, n - pos, deadline);
                if (sent == 0)
                    break;
                pos += sent;
            }
        }
        closeSocketsQuietly(client, upstreamSocket);
    }

    void handleClient(
        dns::Resolver &resolver, engine::io::Socket client, engine::Deadline deadline
    ) noexcept
    {
        if (isClosed())
            return;

        std::string header;
        header.reserve(2048);
        std::array<char, kIoBufferBytes> buf{};
        try {
            while (header.find("\r\n\r\n") == std::string::npos) {
                if (header.size() > kMaxHeaderBytes) {
                    const auto resp = make400Response("header too large");
                    static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
                    client.Close();
                    return;
                }
                const auto n = client.RecvSome(buf.data(), buf.size(), deadline);
                if (n == 0)
                    return;
                header.append(buf.data(), n);
            }
        } catch (const utils::TracefulException &) {
            return;
        }

        const auto parsed = parseHeaderBlock(header);
        if (!parsed) {
            const auto resp = make400Response("invalid request");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }
        if (parsed->target.size() > config.urlBytesMax) {
            const auto resp = make400Response("target too long");
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        const auto auth = findHeaderValue(parsed->headers, "proxy-authorization");
        const auto user = auth ? parseBasicAuthUser(auth.value())
                               : std::optional<std::pair<std::string, std::string>>{};
        if (!user || user->first != config.runId) {
            const auto resp = make407Response();
            static_cast<void>(sendBudgeted(client, resp.data(), resp.size(), deadline));
            return;
        }

        if (absl::EqualsIgnoreCase(parsed->method, "CONNECT")) {
            handleConnect(resolver, client, parsed.value(), deadline);
            return;
        }
        handleHttp(resolver, client, parsed.value(), header, deadline);
    }

    void acceptLoop(dns::Resolver &resolver, engine::Deadline deadline) noexcept
    {
        while (!isClosed()) {
            engine::io::Socket client;
            try {
                client = listener.Accept(deadline);
            } catch (const utils::TracefulException &) {
                return;
            }
            if (isClosed())
                return;
            auto task = engine::AsyncNoSpan([this, &resolver, deadline,
                                             sock = std::move(client)]() mutable noexcept {
                handleClient(resolver, std::move(sock), deadline);
            });
            auto lock = clientTasks.Lock();
            lock->push_back(std::move(task));
        }
    }

    void closeAll() noexcept
    {
        closed.store(true, std::memory_order_relaxed);
        if (acceptTask) {
            acceptTask->RequestCancel();
            static_cast<void>(acceptTask->WaitNothrow());
            acceptTask.reset();
        }
        std::vector<engine::TaskWithResult<void>> tasks;
        {
            auto lock = clientTasks.Lock();
            tasks = std::move(*lock);
            lock->clear();
        }
        for (auto &t : tasks) {
            t.RequestCancel();
        }
        for (auto &t : tasks) {
            static_cast<void>(t.WaitNothrow());
        }
        try {
            if (listener.IsValid())
                listener.Close();
        } catch (const utils::TracefulException &) {
        }
    }
};

EgressProxy::EgressProxy(EgressProxyConfig config) : impl(std::move(config))
{
    UINVARIANT(!impl->config.socketPath.empty(), "proxy socket path must not be empty");
    UINVARIANT(!impl->config.runId.empty(), "proxy runId must not be empty");
    UINVARIANT(impl->config.urlBytesMax > 0UL, "proxy urlBytesMax must be positive");
    UINVARIANT(impl->config.downBytesMax > 0_i64, "proxy downBytesMax must be positive");
}

EgressProxy::~EgressProxy() noexcept { close(); }

Expected<void, String> EgressProxy::start(dns::Resolver &resolver, engine::Deadline deadline)
{
    UINVARIANT(deadline.IsReachable(), "proxy start deadline must be reachable");
    if (impl->acceptTask)
        return std::unexpected("proxy already started"_t);

    impl->listener = engine::io::Socket(
        engine::io::AddrDomain::kUnix, engine::io::SocketType::kStream
    );
    try {
        auto addr = engine::io::Sockaddr::MakeUnixSocketAddress(impl->config.socketPath);
        impl->listener.Bind(addr);
        impl->listener.Listen();
    } catch (const utils::TracefulException &e) {
        return std::unexpected(text::format("proxy bind/listen failed: {}", e.what()));
    }

    impl->acceptTask = engine::AsyncNoSpan([this, &resolver, deadline]() noexcept {
        impl->acceptLoop(resolver, deadline);
    });
    return {};
}

void EgressProxy::close() noexcept { impl->closeAll(); }

i64 EgressProxy::downBytes() const noexcept { return i64(impl->downBytes.load()); }

std::optional<String> EgressProxy::failureReason() const noexcept
{
    auto lock = impl->failure.Lock();
    return *lock;
}

} // namespace v1::crawler
