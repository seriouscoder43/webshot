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

[[nodiscard]] Expected<Url, StorageUrlError> buildCaptureDownloadUrl(
    uuidu::Uuid uuid, S3Mode s3Mode, const String &publicBaseUrl,
    const std::optional<String> &requestHost
);

[[nodiscard]] String storageUrlErrorMessage(StorageUrlError error);

} // namespace v1
