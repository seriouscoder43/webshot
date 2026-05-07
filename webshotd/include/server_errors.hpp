#pragma once

#include "text.hpp"

#include <optional>

namespace ws::errors {

enum class CrudError {
    kDbError,
};

enum class PageTokenError {
    kInvalid,
    kMismatched,
};

enum class CapturePageError {
    kInvalidPageToken,
    kMismatchedPageToken,
    kDbError,
};

enum class CreateJobError {
    kDbError,
};

enum class CaptureErrorKind {
    kCrawler,
    kSizeLimit,
    kPersistMetadataFailed,
};

struct [[nodiscard]] CaptureError {
    CaptureErrorKind kind;
    std::optional<String> detail;
};

} // namespace ws::errors
