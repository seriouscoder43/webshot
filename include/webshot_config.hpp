#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace us = userver;

namespace v1 {
/**
 * @brief Read‑only configuration facade for the service.
 *
 * Exposes knobs used across handlers and the crawler pipeline.
 */
class [[nodiscard]] WebshotConfig final : public us::components::ComponentBase {
public:
    static constexpr std::string_view kName = "webshot-config";

    WebshotConfig(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    [[nodiscard]] static us::yaml_config::Schema GetStaticConfigSchema();

    /** @return Maximum allowed length of URL query part. */
    [[nodiscard]] size_t queryPartLengthMax() const noexcept { return queryPartLengthMaxValue; }

    /** @name S3 parameters */
    ///@{
    [[nodiscard]] const std::string &s3Bucket() const noexcept { return s3BucketName; }
    [[nodiscard]] const std::string &s3Endpoint() const noexcept { return s3EndpointUrl; }
    [[nodiscard]] const std::string &s3Region() const noexcept { return s3RegionName; }
    [[nodiscard]] const std::string &publicBaseUrl() const noexcept { return publicBaseUrlValue; }
    [[nodiscard]] std::chrono::milliseconds s3Timeout() const noexcept { return s3TimeoutDuration; }
    ///@}

private:
    size_t queryPartLengthMaxValue;
    std::string s3BucketName;
    std::string s3EndpointUrl;
    std::string s3RegionName;
    std::string publicBaseUrlValue;
    std::chrono::milliseconds s3TimeoutDuration;
};
} // namespace v1
