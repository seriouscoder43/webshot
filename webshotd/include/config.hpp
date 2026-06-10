#pragma once

#include "integers.hpp"
#include "text.hpp"

#include "chrono.hpp"
#include <string>
#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace ws {
namespace us = userver;

enum class Mode {
    kLocal,
    kExternal,
};

/**
 * @brief Read-only configuration facade for the service.
 *
 * Exposes knobs used across handlers and the crawler pipeline.
 */
class [[nodiscard]] Config final : public us::components::ComponentBase {
public:
    static constexpr std::string_view kName = "config";

    Config(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    [[nodiscard]] static us::yaml_config::Schema GetStaticConfigSchema();

    /** @return Maximum URL byte length accepted when parsing a Link. */
    [[nodiscard]] usize UrlBytesMax() const noexcept { return url_bytes_max_; }

    /** @return Whether captures must match the allowlist. */
    [[nodiscard]] bool AllowlistOnly() const noexcept { return allowlist_only_; }

    /** @return Whether crawler egress is restricted to HTTPS/WSS. */
    [[nodiscard]] bool HttpsOnly() const noexcept { return https_only_; }

    /** @return Whether the crawler should reject non-UTF-8 main document bodies. */
    [[nodiscard]] bool CrawlerAssertUrlIsTextPage() const noexcept
    {
        return crawler_assert_url_is_text_page_;
    }

    /** @return Runner-owned state directory for webshotd instance. */
    [[nodiscard]] std::string_view StateDir() const noexcept { return state_dir_; }

    /** @name S3 parameters */
    ///@{
    [[nodiscard]] Mode S3Mode() const noexcept { return s3_mode_; }
    [[nodiscard]] const String &S3Bucket() const noexcept { return s3_bucket_name_; }
    [[nodiscard]] const String &S3Endpoint() const noexcept { return s3_endpoint_url_; }
    [[nodiscard]] const String &S3Region() const noexcept { return s3_region_name_; }
    [[nodiscard]] const String &S3PublicBaseUrl() const noexcept { return public_base_url_; }
    [[nodiscard]] ws::chrono::milliseconds S3Timeout() const noexcept
    {
        return s3_timeout_duration_;
    }
    ///@}

private:
    usize url_bytes_max_;
    bool allowlist_only_;
    bool https_only_;
    bool crawler_assert_url_is_text_page_;
    std::string state_dir_;
    enum Mode s3_mode_;
    String s3_bucket_name_;
    String s3_endpoint_url_;
    String s3_region_name_;
    String public_base_url_;
    ws::chrono::milliseconds s3_timeout_duration_;
};
} // namespace ws
