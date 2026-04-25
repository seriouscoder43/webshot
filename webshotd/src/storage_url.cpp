#include "storage_url.hpp"

/**
 * @file
 * @brief Helpers for externally visible direct S3 object URLs.
 */

#include "text.hpp"
#include "try.hpp"
#include "userver_namespaces.hpp"
#include "uuid_format.hpp"

#include <format>
#include <string>

using namespace text::literals;

namespace v1 {
namespace {

using enum StorageUrlError;

[[nodiscard]] String appendCaptureFilename(const Url &baseUrl, uuidu::Uuid uuid)
{
    std::string path{baseUrl.pathname().view()};
    if (path.empty())
        path = "/";
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();
    if (path.back() != '/')
        path.push_back('/');
    path += std::format("{}.wacz", uuid);
    return String::fromBytes(path).expect();
}

[[nodiscard]] Expected<String, StorageUrlError>
parseRequestHostname(const std::optional<String> &requestHost)
{
    const auto requestHostValue = TRY_OK_OR(requestHost, kMissingRequestHost);
    ENSURE(!requestHostValue.empty(), kMissingRequestHost);

    const auto parsed = TRY_OK_OR(
        Url::fromText(text::format("http://{}", requestHostValue)), kInvalidRequestHost
    );
    ENSURE(parsed.hasHostname(), kInvalidRequestHost);
    ENSURE(parsed.pathname() == "/"_t, kInvalidRequestHost);
    ENSURE(!parsed.hasSearch(), kInvalidRequestHost);

    return parsed.hostname();
}

[[nodiscard]] Expected<Url, StorageUrlError>
buildConfiguredCaptureDownloadUrl(uuidu::Uuid uuid, const String &publicBaseUrl)
{
    const auto downloadUrlText =
        String::fromBytes(std::format("{}/{}.wacz", publicBaseUrl, uuid)).expect();
    return TRY_OK_OR(Url::fromText(downloadUrlText), kInvalidPublicBaseUrl);
}

} // namespace

Expected<Url, StorageUrlError> buildCaptureDownloadUrl(
    uuidu::Uuid uuid, S3Mode s3Mode, const String &publicBaseUrl,
    const std::optional<String> &requestHost
)
{
    using enum S3Mode;

    if (s3Mode == kExternal)
        return buildConfiguredCaptureDownloadUrl(uuid, publicBaseUrl);

    const auto baseUrl = TRY_OK_OR(Url::fromText(publicBaseUrl), kInvalidPublicBaseUrl);
    ENSURE(baseUrl.isHttpOrHttps(), kInvalidPublicBaseUrl);

    const auto hostname = TRY(parseRequestHostname(requestHost));

    auto downloadUrl = baseUrl.withHostname(hostname);
    downloadUrl = downloadUrl.withPathname(appendCaptureFilename(baseUrl, uuid));
    downloadUrl = downloadUrl.withoutSearch().withoutHash();
    return downloadUrl;
}

String storageUrlErrorMessage(StorageUrlError error)
{
    using enum StorageUrlError;

    switch (error) {
    case kInvalidPublicBaseUrl:
        return "invalid public_base_url"_t;
    case kMissingRequestHost:
        return "missing request Host header"_t;
    case kInvalidRequestHost:
        return "invalid request Host header"_t;
    default:
        invariant(false, "");
        return {};
    }
}

} // namespace v1
