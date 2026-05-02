#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace arkhiv {

enum class ZipArchiveErrorCode : uint8_t {
    kNone,
    kWriterInitFailed,
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
    kWriteHeaderFailed,
    kWriteDataFailed,
    kFinishFailed,
};

struct [[nodiscard]] ZipArchiveError {
    ZipArchiveErrorCode code = ZipArchiveErrorCode::kNone;
    std::string detail;
};

class [[nodiscard]] ZipArchiveBuilder {
public:
    [[nodiscard]] bool
    AddStoredFile(std::string_view path, ZipArchiveError &error_out, std::string_view body);

    [[nodiscard]] std::optional<std::string> Finish(ZipArchiveError &error_out) const;

private:
    struct [[nodiscard]] StoredEntry {
        std::string path;
        std::string body;
    };

    std::vector<StoredEntry> entries_;
    std::set<std::string, std::less<>> entry_paths_;
};

class [[nodiscard]] ZipArchive {
public:
    [[nodiscard]] static std::optional<ZipArchive>
    FromBytes(std::string_view bytes, ZipArchiveError &error_out);

    [[nodiscard]] std::optional<std::string_view> FindFile(std::string_view path) const noexcept;

    [[nodiscard]] const std::vector<std::string> &EntryPathsInOrder() const noexcept;

private:
    ZipArchive(
        std::map<std::string, std::string, std::less<>> files, std::vector<std::string> paths
    );

    std::map<std::string, std::string, std::less<>> files_;
    std::vector<std::string> paths_;
};

} // namespace arkhiv
