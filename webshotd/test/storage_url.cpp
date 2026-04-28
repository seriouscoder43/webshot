#include "storage_url.hpp"

#include <userver/utest/utest.hpp>

#include <boost/uuid/string_generator.hpp>

using namespace v1;
using namespace text::literals;
using enum S3Mode;
using enum StorageUrlError;

namespace {

[[nodiscard]] uuidu::Uuid sampleUuid()
{
    return boost::uuids::string_generator{}("123e4567-e89b-12d3-a456-426614174000");
}

} // namespace

UTEST(StorageUrl, ExternalModePreservesConfiguredBase)
{
    const auto url = buildCaptureDownloadUrl(
        sampleUuid(), kExternal, "https://storage.example/webshot"_t, "client.example:8080"_t
    );

    ASSERT_TRUE(url);
    EXPECT_EQ(
        url->href(), "https://storage.example/webshot/123e4567-e89b-12d3-a456-426614174000.wacz"_t
    );
}

UTEST(StorageUrl, LocalModeUsesRequestHostnameAndConfiguredS3Port)
{
    const auto url = buildCaptureDownloadUrl(
        sampleUuid(), kLocal, "http://127.0.0.1:8333/webshot"_t, "client.example:8080"_t
    );

    ASSERT_TRUE(url);
    EXPECT_EQ(
        url->href(),
        "http://client.example:8333/webshot/123e4567-e89b-12d3-a456-426614174000.wacz"_t
    );
}

UTEST(StorageUrl, LocalModeHandlesBracketedIpv6RequestHost)
{
    const auto url = buildCaptureDownloadUrl(
        sampleUuid(), kLocal, "http://127.0.0.1:8333/webshot"_t, "[2001:db8::1]:8080"_t
    );

    ASSERT_TRUE(url);
    EXPECT_EQ(
        url->href(), "http://[2001:db8::1]:8333/webshot/123e4567-e89b-12d3-a456-426614174000.wacz"_t
    );
}

UTEST(StorageUrl, LocalModeRejectsMissingRequestHost)
{
    const auto url = buildCaptureDownloadUrl(
        sampleUuid(), kLocal, "http://127.0.0.1:8333/webshot"_t, {}
    );

    ASSERT_FALSE(url);
    EXPECT_EQ(url.error(), kMissingRequestHost);
}

UTEST(StorageUrl, LocalModeRejectsInvalidRequestHost)
{
    const auto url = buildCaptureDownloadUrl(
        sampleUuid(), kLocal, "http://127.0.0.1:8333/webshot"_t, "client.example/path"_t
    );

    ASSERT_FALSE(url);
    EXPECT_EQ(url.error(), kInvalidRequestHost);
}
