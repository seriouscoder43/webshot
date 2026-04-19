#include "crawler/cdp_client.hpp"

#include "grab_value.hpp"
#include "schema/cdp.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <format>

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>

#include <userver/crypto/base64.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/crypto/random.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/engine/exception.hpp>
#include <userver/engine/io/sockaddr.hpp>
#include <userver/engine/io/socket.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/traceful_exception.hpp>
#include <userver/utils/underlying_value.hpp>
#include <userver/websocket/connection.hpp>
namespace chrono = std::chrono;

namespace v1::crawler {
using namespace text::literals;
using v1::Expected;

namespace {

constexpr auto kMaxHandshakeResponseBytes = 16_i64 * 1024_i64;
constexpr std::string_view kWebsocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

[[nodiscard]] eng::Deadline pickEarlierReachableDeadline(eng::Deadline a, eng::Deadline b)
{
    UINVARIANT(a.IsReachable(), "deadline must be reachable");
    UINVARIANT(b.IsReachable(), "deadline must be reachable");
    return a.TimeLeft() <= b.TimeLeft() ? a : b;
}

[[nodiscard]] std::string currentTraceTimestamp()
{
    return datetime::UtcTimestring(datetime::Now(), datetime::kRfc3339Format);
}

struct HandshakeResponse final {
    std::string statusLine;
    std::unordered_map<std::string, std::string> headers;
    std::string rawHeaders;
};

[[nodiscard]] Expected<String, CdpFailure> getErrorMessage(const json::Value &error)
{
    using enum CdpError;
    UINVARIANT(error.IsObject(), "cdp error payload must be object");
    try {
        auto message = String::fromBytes(error.As<dto::CdpError>().message);
        if (!message)
            return std::unexpected(CdpFailure{.code = kProtocol, .detail = {}});
        return std::move(*message);
    } catch (const json::Exception &e) {
        UINVARIANT(
            false, std::format("cdp error payload does not match dto::CdpError ({})", e.what())
        );
    }
    return std::unexpected(CdpFailure{.code = kProtocol, .detail = {}});
}

[[nodiscard]] bool containsHeaderToken(std::string_view value, std::string_view token)
{
    auto remaining = value;
    while (true) {
        const auto commaPos = remaining.find(',');
        const auto part = commaPos == std::string_view::npos ? remaining
                                                             : remaining.substr(0, commaPos);
        const auto trimmedPart = absl::StripAsciiWhitespace(part);
        if (absl::EqualsIgnoreCase(trimmedPart, token))
            return true;
        if (commaPos == std::string_view::npos)
            break;
        remaining.remove_prefix(commaPos + 1);
    }

    return false;
}

[[nodiscard]] Expected<std::string, CdpFailure>
readHandshakeResponse(eng::io::Socket &socket, eng::Deadline deadline)
{
    using enum CdpError;
    std::string response;
    response.reserve(1024);

    while (response.find("\r\n\r\n") == std::string::npos) {
        if (ssize(response) >= kMaxHandshakeResponseBytes)
            return std::unexpected(CdpFailure{.code = kHandshakeResponseTooLarge, .detail = {}});

        char ch = '\0';
        const auto bytesRead = socket.RecvSome(&ch, 1, deadline);
        if (bytesRead == 0)
            return std::unexpected(CdpFailure{.code = kHandshakeUnexpectedEof, .detail = {}});
        response.push_back(ch);
    }

    return response;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
parseHandshakeHeaders(std::string_view headersBlock)
{
    std::unordered_map<std::string, std::string> headers;

    auto remaining = headersBlock;
    while (!remaining.empty()) {
        const auto lineEnd = remaining.find("\r\n");
        const auto line = lineEnd == std::string::npos ? remaining : remaining.substr(0, lineEnd);
        if (line.empty())
            break;
        const auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            const auto trimmed = absl::StripAsciiWhitespace(line.substr(colonPos + 1));
            headers.emplace(absl::AsciiStrToLower(line.substr(0, colonPos)), std::string(trimmed));
        }

        if (lineEnd == std::string::npos)
            break;
        remaining.remove_prefix(lineEnd + 2);
    }

    return headers;
}

[[nodiscard]] Expected<HandshakeResponse, CdpFailure>
parseHandshakeResponse(std::string_view response)
{
    const auto headersEnd = response.find("\r\n\r\n");
    if (headersEnd == std::string::npos)
        return std::unexpected(
            CdpFailure{.code = CdpError::kHandshakeMalformedResponse, .detail = {}}
        );

    HandshakeResponse parsed;
    parsed.rawHeaders = std::string(response.substr(0, headersEnd + 4));

    const auto statusLineEnd = response.find("\r\n");
    if (statusLineEnd == std::string::npos)
        return std::unexpected(
            CdpFailure{.code = CdpError::kHandshakeMalformedResponse, .detail = {}}
        );

    parsed.statusLine = std::string(response.substr(0, statusLineEnd));
    parsed.headers = parseHandshakeHeaders(
        response.substr(statusLineEnd + 2, headersEnd - (statusLineEnd + 2))
    );
    return parsed;
}

Expected<void, CdpFailure>
validateHandshakeResponse(const HandshakeResponse &response, std::string_view secWebsocketKey)
{
    if (response.statusLine.rfind("HTTP/1.1 101 ", 0) != 0 &&
        response.statusLine != "HTTP/1.1 101") {
        return std::unexpected(CdpFailure{.code = CdpError::kHandshakeRejected, .detail = {}});
    }

    const auto upgradeIt = response.headers.find("upgrade");
    if (upgradeIt == std::end(response.headers) ||
        !absl::EqualsIgnoreCase(std::string_view{upgradeIt->second}, "websocket")) {
        return std::unexpected(CdpFailure{.code = CdpError::kHandshakeMissingHeader, .detail = {}});
    }

    const auto connectionIt = response.headers.find("connection");
    if (connectionIt == std::end(response.headers) ||
        !containsHeaderToken(connectionIt->second, "upgrade")) {
        return std::unexpected(CdpFailure{.code = CdpError::kHandshakeMissingHeader, .detail = {}});
    }

    const auto acceptIt = response.headers.find("sec-websocket-accept");
    if (acceptIt == std::end(response.headers)) {
        return std::unexpected(CdpFailure{.code = CdpError::kHandshakeMissingHeader, .detail = {}});
    }

    const auto expectedAccept = us::crypto::base64::Base64Encode(
        us::crypto::hash::Sha1(
            {secWebsocketKey, kWebsocketGuid}, us::crypto::hash::OutputEncoding::kBinary
        )
    );
    if (acceptIt->second != expectedAccept)
        return std::unexpected(CdpFailure{.code = CdpError::kHandshakeRejected, .detail = {}});
    return {};
}

} // namespace

String describeCdpFailure(const String &action, const CdpFailure &failure)
{
    auto message = action;
    if (failure.detail)
        message = text::format("{}: {}", message, *failure.detail);
    return message;
}

struct CdpSessionState final {
    struct Data final {
        eng::ConditionVariable cv;
        std::deque<CdpEvent> events;
        std::optional<CdpFailure> failure;
        bool closed{false};
    };

    us::concurrent::Variable<Data> data;
};

struct CdpClient::PendingCommandWaiter final {
    struct Data final {
        eng::ConditionVariable cv;
        std::optional<json::Value> result;
        std::optional<CdpFailure> failure;
        bool done{false};
    };

    us::concurrent::Variable<Data> data;
};

namespace {

[[nodiscard]] String parsePrintableText(std::string_view value)
{
    return String::fromBytes(value).expect();
}

[[nodiscard]] CdpFailure makeTimeoutFailure(const String &timeoutMessage)
{
    return {
        .code = CdpError::kTimeout,
        .detail = std::optional<String>{timeoutMessage},
    };
}

[[nodiscard]] CdpFailure makeSocketClosedFailure(const String &detail)
{
    return {
        .code = CdpError::kSocketClosed,
        .detail = std::optional<String>{detail},
    };
}

[[nodiscard]] std::optional<String>
extractRoutingSessionId(const dto::CdpEventMessage &eventMessage)
{
    if (eventMessage.sessionId) {
        return String::fromBytes(*eventMessage.sessionId).expect();
    }
    if (!eventMessage.params)
        return {};
    const auto sessionIdValue = eventMessage.params->extra["sessionId"];
    if (sessionIdValue.IsMissing())
        return {};
    const auto sessionId = String::fromBytes(sessionIdValue.As<std::string>());
    if (!sessionId)
        return {};
    return *sessionId;
}

[[nodiscard]] std::optional<String> extractRoutingTargetId(const dto::CdpEventMessage &eventMessage)
{
    if (!eventMessage.params)
        return {};
    const auto targetIdValue = eventMessage.params->extra["targetId"];
    if (targetIdValue.IsMissing())
        return {};
    const auto targetId = String::fromBytes(targetIdValue.As<std::string>());
    if (!targetId)
        return {};
    return *targetId;
}

} // namespace

CdpClient::CdpClient(
    std::string socketPath, String websocketPath,
    std::shared_ptr<us::websocket::WebSocketConnection> connection, std::string tracePath,
    us::fs::blocking::FileDescriptor traceFile, eng::Deadline overallDeadline,
    chrono::milliseconds commandTimeout
)
    : socketPath(std::move(socketPath)), websocketPath(std::move(websocketPath)),
      connection(std::move(connection)), tracePath(std::move(tracePath)),
      traceFile(std::move(traceFile)), overallDeadline(overallDeadline),
      commandTimeout(commandTimeout)
{
}

Expected<std::unique_ptr<CdpClient>, CdpFailure> CdpClient::connect(
    std::string socketPath, String websocketPath, std::string tracePath,
    eng::Deadline overallDeadline, chrono::milliseconds handshakeTimeout,
    chrono::milliseconds commandTimeout, i64 maxRemotePayloadBytes
)
{
    using enum CdpError;

    UINVARIANT(!tracePath.empty(), "cdp trace path must not be empty");
    UINVARIANT(overallDeadline.IsReachable(), "cdp overall deadline must be reachable");

    us::fs::blocking::FileDescriptor traceFd;
    try {
        traceFd = us::fs::blocking::FileDescriptor::Open(
            tracePath, us::fs::blocking::OpenMode{
                           us::fs::blocking::OpenFlag::kWrite,
                           us::fs::blocking::OpenFlag::kCreateIfNotExists,
                           us::fs::blocking::OpenFlag::kAppend,
                       }
        );
    } catch (const std::runtime_error &) {
        return std::unexpected(CdpFailure{.code = kTraceFileOpenFailed, .detail = {}});
    }

    auto socket = eng::io::Socket{eng::io::AddrDomain::kUnix, eng::io::SocketType::kStream};
    const auto handshakeDeadline = pickEarlierReachableDeadline(
        overallDeadline, eng::Deadline::FromDuration(handshakeTimeout)
    );
    auto address = eng::io::Sockaddr::MakeUnixSocketAddress(socketPath);
    try {
        socket.Connect(address, handshakeDeadline);
    } catch (const us::utils::TracefulException &) {
        return std::unexpected(CdpFailure{.code = kSocketConnectFailed, .detail = {}});
    }

    auto randomKey = std::array<char, 16>{};
    us::crypto::GenerateRandomBlock(us::utils::span(randomKey));
    const auto secWebsocketKey = us::crypto::base64::Base64Encode(
        std::string_view(randomKey.data(), randomKey.size())
    );

    const auto request = std::format(
        "GET {} HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: {}\r\n"
        "\r\n",
        websocketPath.empty()
            ? "/"
            : (websocketPath.startsWith('/') ? websocketPath.view()
                                             : ("/" + std::string(websocketPath.view()))),
        secWebsocketKey
    );
    try {
        static_cast<void>(socket.SendAll(request.data(), request.size(), handshakeDeadline));
    } catch (const us::utils::TracefulException &) {
        return std::unexpected(CdpFailure{.code = kTransport, .detail = {}});
    }

    auto response = readHandshakeResponse(socket, handshakeDeadline);
    if (!response)
        return std::unexpected(response.error());
    auto parsedResponse = parseHandshakeResponse(grabValueOf(response));
    if (!parsedResponse)
        return std::unexpected(parsedResponse.error());
    auto validated = validateHandshakeResponse(grabValueOf(parsedResponse), secWebsocketKey);
    if (!validated)
        return std::unexpected(validated.error());

    std::shared_ptr<us::websocket::WebSocketConnection> ws;
    try {
        auto connectionSocket = std::make_unique<eng::io::Socket>(std::move(socket));
        us::websocket::Config wsConfig;
        wsConfig.max_remote_payload = numericCast<decltype(wsConfig.max_remote_payload)>(
            maxRemotePayloadBytes
        );
        ws = us::websocket::MakeClientWebSocketConnection(
            std::move(connectionSocket), std::move(address), wsConfig
        );
    } catch (const us::utils::TracefulException &) {
        return std::unexpected(CdpFailure{.code = kTransport, .detail = {}});
    }

    auto client = std::unique_ptr<CdpClient>(new CdpClient(
        std::move(socketPath), std::move(websocketPath), std::move(ws), std::move(tracePath),
        std::move(traceFd), overallDeadline, commandTimeout
    ));
    client->startReaderTask();
    return client;
}

CdpClient::~CdpClient() noexcept { closeQuietly(); }

void CdpClient::startReaderTask()
{
    readerTask = std::move(eng::CriticalAsyncNoSpan([this]() { readerLoop(); })).AsTask();
}

Expected<json::Value, CdpFailure> CdpClient::sendRaw(
    const String &method, const json::Value &params, const std::optional<String> &sessionId
)
{
    using enum CdpError;
    auto waiter = std::make_shared<PendingCommandWaiter>();
    i64 id{0_i64};

    {
        auto state = sharedState.Lock();
        if (state->terminalFailure)
            return std::unexpected(*state->terminalFailure);
        if (state->closing || state->closed)
            return std::unexpected(makeSocketClosedFailure("cdp socket is closed"_t));
        id = state->nextRequestId++;
        state->pendingRequests.insertWaiting(id, method, sessionId);
        state->pendingWaiters.emplace(id, waiter);
    }

    dto::CdpCommandRequest request;
    request.id = raw(id);
    request.method = std::string(method.view());
    if (!params.IsMissing())
        request.params = dto::CdpCommandRequest::Params{params};
    if (sessionId)
        request.sessionId = std::string(sessionId->view());
    traceCommand(id, method, sessionId);
    try {
        const auto sendLock = sendState.Lock();
        static_cast<void>(sendLock);
        connection->SendText(json::ToString(json::ValueBuilder(request).ExtractValue()));
    } catch (const us::utils::TracefulException &e) {
        traceTransportError("send"_t, parsePrintableText(e.what()));
        auto failure = CdpFailure{.code = kTransport, .detail = {}};
        failTerminal(failure);
        return std::unexpected(failure);
    }

    const auto deadline = pickEarlierReachableDeadline(
        overallDeadline, eng::Deadline::FromDuration(commandTimeout)
    );
    auto waiterState = waiter->data.UniqueLock();
    const auto ready = [&waiterState]() { return waiterState->done; };
    if (!ready() && !waiterState->cv.WaitUntil(waiterState.GetLock(), deadline, ready)) {
        waiterState.GetLock().unlock();
        {
            auto state = sharedState.Lock();
            if (auto *requestInFlight = state->pendingRequests.find(id);
                requestInFlight != nullptr) {
                requestInFlight->ignoreResponse = true;
                state->pendingWaiters.erase(id);
                return std::unexpected(makeTimeoutFailure("timed out waiting for cdp response"_t));
            }
        }
        waiterState.GetLock().lock();
    }
    if (!waiterState->done)
        return std::unexpected(makeTimeoutFailure("timed out waiting for cdp response"_t));
    if (waiterState->failure)
        return std::unexpected(*waiterState->failure);
    UINVARIANT(waiterState->result, "cdp waiter completed without result");
    return *waiterState->result;
}

Expected<void, CdpFailure> CdpClient::close()
{
    {
        auto state = sharedState.Lock();
        if (state->closed || state->terminalFailure)
            return {};
        state->closing = true;
    }

    traceClose("out"_t, numericCast<int>(us::websocket::CloseStatus::kNormal));
    try {
        const auto sendLock = sendState.Lock();
        static_cast<void>(sendLock);
        if (connection)
            connection->Close(us::websocket::CloseStatus::kNormal);
    } catch (const us::utils::TracefulException &e) {
        traceTransportError("close"_t, parsePrintableText(e.what()));
        auto failure = CdpFailure{.code = CdpError::kTransport, .detail = {}};
        failTerminal(failure);
        connection.reset();
        return std::unexpected(failure);
    }

    if (readerTask.IsValid()) {
        readerTask.RequestCancel();
        const eng::TaskCancellationBlocker blocker;
        static_cast<void>(readerTask.WaitNothrow());
    }
    connection.reset();
    return {};
}

Expected<std::unique_ptr<CdpSession>, CdpFailure>
CdpClient::createSession(String sessionId, String targetId)
{
    auto sessionState = std::make_shared<CdpSessionState>();
    {
        auto state = sharedState.Lock();
        if (state->terminalFailure)
            return std::unexpected(*state->terminalFailure);
        if (state->closing || state->closed)
            return std::unexpected(makeSocketClosedFailure("cdp socket is closed"_t));
        state->sessionsById.emplace(sessionId, sessionState);
        state->sessionsByTargetId.emplace(targetId, sessionState);
    }
    return std::unique_ptr<CdpSession>(
        new CdpSession(*this, std::move(sessionId), std::move(targetId), std::move(sessionState))
    );
}

void CdpClient::readerLoop()
{
    while (true) {
        us::websocket::Message message;
        try {
            connection->Recv(message);
        } catch (const eng::WaitInterruptedException &) {
            failTerminal(makeSocketClosedFailure("websocket close requested"_t));
            return;
        } catch (const us::utils::TracefulException &e) {
            traceTransportError("recv"_t, parsePrintableText(e.what()));
            failTerminal(CdpFailure{.code = CdpError::kTransport, .detail = {}});
            return;
        }
        if (message.close_status) {
            const auto closeCode = us::utils::UnderlyingValue(*message.close_status);
            traceClose("in"_t, closeCode);
            failTerminal(
                CdpFailure{
                    .code = CdpError::kSocketClosed,
                    .detail = text::format("websocket close status {}", closeCode),
                }
            );
            return;
        }
        auto ok = handleMessage(message.data);
        if (!ok) {
            failTerminal(ok.error());
            return;
        }
    }
}

CdpSession::~CdpSession()
{
    if (client == nullptr || sessionState == nullptr)
        return;
    client->unregisterSession(sessionIdValue, targetIdValue, sessionState);
}

Expected<void, CdpFailure> CdpClient::handleMessage(const std::string &payload)
{
    using enum CdpError;
    json::Value value;
    try {
        value = json::FromString(payload);
    } catch (const json::Exception &) {
        return std::unexpected(CdpFailure{.code = kJsonParseFailed, .detail = {}});
    }
    const auto idValue = value["id"];
    if (!idValue.IsMissing()) {
        const auto id = i64(idValue.As<int64_t>());
        std::shared_ptr<PendingCommandWaiter> waiter;
        std::optional<String> errorMessage;
        CdpPendingRequest requestCopy;
        {
            auto state = sharedState.Lock();
            auto *request = state->pendingRequests.find(id);
            UINVARIANT(
                request != nullptr, std::format("cdp response for unknown request id {}", id)
            );
            requestCopy = *request;
            const auto errorValue = value["error"];
            if (!errorValue.IsMissing()) {
                auto parsed = getErrorMessage(errorValue);
                if (!parsed)
                    return std::unexpected(parsed.error());
                errorMessage = *parsed;
            }
            if (request->ignoreResponse) {
                state->pendingRequests.erase(id);
                state->pendingWaiters.erase(id);
                traceResponse(id, &requestCopy, errorMessage);
                return {};
            }
            const auto waiterIt = state->pendingWaiters.find(id);
            UINVARIANT(
                waiterIt != std::end(state->pendingWaiters),
                std::format("missing cdp waiter for request id {}", id)
            );
            waiter = waiterIt->second;
            state->pendingWaiters.erase(waiterIt);
            state->pendingRequests.erase(id);
        }
        const auto errorValue = value["error"];
        traceResponse(id, &requestCopy, errorMessage);
        auto waiterState = waiter->data.Lock();
        if (errorValue.IsMissing()) {
            waiterState->result = value["result"];
        } else {
            waiterState->failure = CdpFailure{.code = kCommandFailed, .detail = *errorMessage};
        }
        waiterState->done = true;
        waiterState->cv.NotifyOne();
        return {};
    }

    UINVARIANT(!value["method"].IsMissing(), "cdp message missing id and method");

    dto::CdpEventMessage eventMessage;
    try {
        eventMessage = value.As<dto::CdpEventMessage>();
    } catch (const json::Exception &) {
        return std::unexpected(CdpFailure{.code = kProtocol, .detail = {}});
    }
    const auto sessionId = eventMessage.sessionId.transform([](const auto &s) {
        return String::fromBytes(s).expect();
    });
    const auto method = String::fromBytes(eventMessage.method).expect();
    const auto routingSessionId = extractRoutingSessionId(eventMessage);
    const auto routingTargetId = extractRoutingTargetId(eventMessage);
    traceEvent(method, sessionId);

    auto event = CdpEvent{
        .method = method,
        .params = std::move(eventMessage.params),
        .sessionId = sessionId,
    };

    std::shared_ptr<CdpSessionState> sessionState;
    {
        auto state = sharedState.Lock();
        if (routingSessionId) {
            if (const auto it = state->sessionsById.find(*routingSessionId);
                it != std::end(state->sessionsById)) {
                sessionState = it->second;
            }
        }
        if (!sessionState) {
            if (routingTargetId) {
                if (const auto it = state->sessionsByTargetId.find(*routingTargetId);
                    it != std::end(state->sessionsByTargetId)) {
                    sessionState = it->second;
                }
            }
        }
    }
    if (!sessionState)
        return {};

    auto sessionData = sessionState->data.Lock();
    if (sessionData->closed || sessionData->failure)
        return {};
    sessionData->events.push_back(std::move(event));
    sessionData->cv.NotifyOne();
    return {};
}

Expected<CdpEvent, CdpFailure> CdpClient::waitForSessionEvent(
    const std::shared_ptr<CdpSessionState> &sessionState, eng::Deadline deadline,
    const String &timeoutMessage
)
{
    auto sessionData = sessionState->data.UniqueLock();
    const auto ready = [&sessionData]() {
        return !sessionData->events.empty() || sessionData->failure || sessionData->closed;
    };
    if (!ready() && !sessionData->cv.WaitUntil(sessionData.GetLock(), deadline, ready))
        return std::unexpected(makeTimeoutFailure(timeoutMessage));
    if (sessionData->failure)
        return std::unexpected(*sessionData->failure);
    if (sessionData->closed)
        return std::unexpected(makeSocketClosedFailure("cdp session is closed"_t));
    UINVARIANT(!sessionData->events.empty(), "cdp session woke without queued event");
    auto event = std::move(sessionData->events.front());
    sessionData->events.pop_front();
    return event;
}

std::vector<CdpEvent>
CdpClient::drainSessionEvents(const std::shared_ptr<CdpSessionState> &sessionState)
{
    std::vector<CdpEvent> events;
    auto sessionData = sessionState->data.Lock();
    events.reserve(sessionData->events.size());
    while (!sessionData->events.empty()) {
        events.push_back(std::move(sessionData->events.front()));
        sessionData->events.pop_front();
    }
    return events;
}

void CdpClient::unregisterSession(
    const String &sessionId, const String &targetId,
    const std::shared_ptr<CdpSessionState> &sessionState
) noexcept
{
    {
        auto state = sharedState.Lock();
        if (const auto it = state->sessionsById.find(sessionId);
            it != std::end(state->sessionsById) && it->second == sessionState) {
            state->sessionsById.erase(it);
        }
        if (const auto it = state->sessionsByTargetId.find(targetId);
            it != std::end(state->sessionsByTargetId) && it->second == sessionState) {
            state->sessionsByTargetId.erase(it);
        }
    }
    auto sessionData = sessionState->data.Lock();
    sessionData->closed = true;
    if (!sessionData->failure)
        sessionData->failure = makeSocketClosedFailure("cdp session is closed"_t);
    sessionData->cv.NotifyAll();
}

void CdpClient::closeQuietly() noexcept
{
    auto state = sharedState.Lock();
    state->closing = true;
    state->closed = true;
    if (readerTask.IsValid())
        readerTask.RequestCancel();
}

void CdpClient::failTerminal(CdpFailure failure)
{
    std::vector<std::shared_ptr<PendingCommandWaiter>> waiters;
    std::vector<std::shared_ptr<CdpSessionState>> sessions;
    {
        auto state = sharedState.Lock();
        if (state->terminalFailure)
            return;
        state->terminalFailure = failure;
        state->closed = true;
        for (auto &[_, waiter] : state->pendingWaiters)
            waiters.push_back(waiter);
        state->pendingWaiters.clear();
        state->pendingRequests.clear();
        for (auto &[_, sessionState] : state->sessionsById)
            sessions.push_back(sessionState);
        state->sessionsById.clear();
        state->sessionsByTargetId.clear();
    }

    for (const auto &waiter : waiters) {
        auto waiterState = waiter->data.Lock();
        waiterState->failure = failure;
        waiterState->done = true;
        waiterState->cv.NotifyOne();
    }
    for (const auto &sessionState : sessions) {
        auto sessionData = sessionState->data.Lock();
        sessionData->failure = failure;
        sessionData->closed = true;
        sessionData->cv.NotifyAll();
    }
}

Expected<CdpEvent, CdpFailure>
CdpSession::waitEvent(eng::Deadline deadline, const String &timeoutMessage)
{
    UINVARIANT(client != nullptr, "cdp session is not attached");
    UINVARIANT(sessionState != nullptr, "cdp session state is missing");
    return client->waitForSessionEvent(sessionState, deadline, timeoutMessage);
}

std::vector<CdpEvent> CdpSession::drainAvailableEvents()
{
    UINVARIANT(client != nullptr, "cdp session is not attached");
    UINVARIANT(sessionState != nullptr, "cdp session state is missing");
    return client->drainSessionEvents(sessionState);
}

Expected<void, CdpFailure> CdpClient::writeTraceLine(const json::Value &value)
{
    using enum CdpError;
    auto line = json::ToString(value);
    line.push_back('\n');
    try {
        traceFile.Write(line);
    } catch (const std::runtime_error &) {
        return std::unexpected(CdpFailure{.code = kTraceWriteFailed, .detail = {}});
    }
    return {};
}

void CdpClient::traceCommand(i64 id, const String &method, const std::optional<String> &sessionId)
{
    json::ValueBuilder entry;
    entry["ts"] = currentTraceTimestamp();
    entry["direction"] = "out";
    entry["kind"] = "command";
    entry["id"] = raw(id);
    entry["method"] = std::string(method.view());
    if (sessionId)
        entry["sessionId"] = std::string(sessionId->view());
    writeTraceLine(entry.ExtractValue()).expect();
}

void CdpClient::traceResponse(
    i64 id, const CdpPendingRequest *request, const std::optional<String> &error
)
{
    json::ValueBuilder entry;
    entry["ts"] = currentTraceTimestamp();
    entry["direction"] = "in";
    entry["kind"] = error ? "error" : "response";
    entry["id"] = raw(id);
    if (request) {
        entry["method"] = std::string(request->method.view());
        if (request->sessionId)
            entry["sessionId"] = std::string(request->sessionId->view());
    }
    if (error)
        entry["error"] = std::string(error->view());
    writeTraceLine(entry.ExtractValue()).expect();
}

void CdpClient::traceEvent(const String &method, const std::optional<String> &sessionId)
{
    json::ValueBuilder entry;
    entry["ts"] = currentTraceTimestamp();
    entry["direction"] = "in";
    entry["kind"] = "event";
    entry["method"] = std::string(method.view());
    if (sessionId)
        entry["sessionId"] = std::string(sessionId->view());
    writeTraceLine(entry.ExtractValue()).expect();
}

void CdpClient::traceClose(const String &direction, int closeCode)
{
    json::ValueBuilder entry;
    entry["ts"] = currentTraceTimestamp();
    entry["direction"] = std::string(direction.view());
    entry["kind"] = "close";
    entry["closeCode"] = closeCode;
    writeTraceLine(entry.ExtractValue()).expect();
}

void CdpClient::traceTransportError(const String &operation, const String &error)
{
    json::ValueBuilder entry;
    entry["ts"] = currentTraceTimestamp();
    entry["kind"] = "transport_error";
    entry["operation"] = std::string(operation.view());
    entry["error"] = std::string(error.view());
    writeTraceLine(entry.ExtractValue()).expect();
}

} // namespace v1::crawler
