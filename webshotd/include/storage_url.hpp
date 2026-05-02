#pragma once

#include "config.hpp"
#include "expected.hpp"
#include "text.hpp"
#include "url.hpp"
#include "uuid_utils.hpp"

#include <optional>

namespace v1 {

enum class StorageUrlError {
    kInvalidPublicBaseUrl,
    kMissingRequestHost,
    kInvalidRequestHost,
};

[[nodiscard]] Expected<Url, StorageUrlError> BuildCaptureDownloadUrl(
    uuidu::Uuid uuid, S3Mode s3_mode, const String &public_base_url,
    const std::optional<String> &request_host
);

[[nodiscard]] String StorageUrlErrorMessage(StorageUrlError error);

} // namespace v1
