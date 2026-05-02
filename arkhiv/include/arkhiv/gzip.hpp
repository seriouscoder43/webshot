#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arkhiv {

enum class GzipErrorCode : uint8_t {
    kNone,
    kWriterInitFailed,
    kOpenFailed,
    kWriteHeaderFailed,
    kWriteDataFailed,
    kFinishFailed,
};

struct [[nodiscard]] GzipError {
    GzipErrorCode code = GzipErrorCode::kNone;
    std::string detail;
};

// Produces a single gzip "member" containing exactly `body`.
[[nodiscard]] std::optional<std::string>
GzipCompressMember(std::string_view body, GzipError &error_out);

} // namespace arkhiv
