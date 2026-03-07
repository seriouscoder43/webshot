#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace arkhiv {

enum class TarArchiveErrorCode {
    kNone,
    kReaderInitFailed,
    kOpenFailed,
    kMissingPath,
    kInvalidPath,
    kUnsupportedEntryType,
    kInvalidEntrySize,
    kEntryTooLarge,
    kReadDataFailed,
    kTruncatedEntry,
    kDuplicateEntry,
    kReadHeaderFailed,
};

struct [[nodiscard]] TarArchiveError {
    TarArchiveErrorCode code = TarArchiveErrorCode::kNone;
    std::string detail;
};

class [[nodiscard]] TarArchive {
public:
    [[nodiscard]] static std::optional<TarArchive>
    fromBytes(std::string_view bytes, TarArchiveError &errorOut);

    [[nodiscard]] std::optional<std::string_view> findFile(std::string_view path) const noexcept;

private:
    explicit TarArchive(std::map<std::string, std::string, std::less<>> filesIn);

    std::map<std::string, std::string, std::less<>> files;
};

} // namespace arkhiv
