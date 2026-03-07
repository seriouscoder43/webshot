#include "arkhiv/tar_archive.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <archive.h>
#include <archive_entry.h>

namespace arkhiv {

namespace {

using ArchivePtr = std::unique_ptr<archive, decltype(&archive_read_free)>;

template <typename T>
[[nodiscard]] std::optional<T>
fail(TarArchiveError &errorOut, TarArchiveErrorCode code, std::string detail)
{
    errorOut.code = code;
    errorOut.detail = std::move(detail);
    return {};
}

[[nodiscard]] std::string formatArchiveDetail(archive *reader, std::string_view context)
{
    auto detail = std::string(context);
    const auto *archiveDetail = archive_error_string(reader);
    if (archiveDetail != nullptr) {
        detail += ": ";
        detail += archiveDetail;
    }
    return detail;
}

[[nodiscard]] std::optional<ArchivePtr> makeReader(TarArchiveError &errorOut)
{
    auto *reader = archive_read_new();
    if (reader == nullptr)
        return fail<ArchivePtr>(
            errorOut, TarArchiveErrorCode::kReaderInitFailed, "failed to allocate tar reader"
        );

    auto archivePtr = ArchivePtr(reader, &archive_read_free);
    if (archive_read_support_format_tar(archivePtr.get()) != ARCHIVE_OK) {
        return fail<ArchivePtr>(
            errorOut, TarArchiveErrorCode::kReaderInitFailed,
            formatArchiveDetail(archivePtr.get(), "failed to enable tar reader")
        );
    }

    return archivePtr;
}

[[nodiscard]] bool isAllowedPath(std::string_view path) noexcept
{
    if (path.empty() || path == "." || path == "..")
        return false;
    if (path.front() == '/' || path.front() == '\\')
        return false;
    if (path.back() == '/' || path.back() == '\\')
        return false;

    for (char ch : path) {
        if (ch == '/' || ch == '\\')
            return false;
        if (static_cast<unsigned char>(ch) < 0x20U || ch == 0x7f)
            return false;
    }

    return true;
}

[[nodiscard]] std::optional<std::string>
readEntryBytes(archive *reader, archive_entry *entry, TarArchiveError &errorOut)
{
    const auto entrySize = archive_entry_size(entry);
    if (entrySize < 0)
        return fail<std::string>(
            errorOut, TarArchiveErrorCode::kInvalidEntrySize, "tar entry size must not be negative"
        );
    if (static_cast<uint64_t>(entrySize) >
        static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        return fail<std::string>(
            errorOut, TarArchiveErrorCode::kEntryTooLarge, "tar entry is too large to fit in memory"
        );

    auto data = std::string(static_cast<size_t>(entrySize), '\0');
    size_t copied = 0;

    while (copied < data.size()) {
        const auto rc = archive_read_data(reader, data.data() + copied, data.size() - copied);
        if (rc < 0) {
            return fail<std::string>(
                errorOut, TarArchiveErrorCode::kReadDataFailed,
                formatArchiveDetail(reader, "failed to read tar entry body")
            );
        }
        if (rc == 0)
            break;
        copied += static_cast<size_t>(rc);
    }

    if (copied != data.size())
        return fail<std::string>(
            errorOut, TarArchiveErrorCode::kTruncatedEntry, "tar entry body was truncated"
        );

    return data;
}

} // namespace

TarArchive::TarArchive(std::map<std::string, std::string, std::less<>> filesIn)
    : files(std::move(filesIn))
{
}

std::optional<TarArchive> TarArchive::fromBytes(std::string_view bytes, TarArchiveError &errorOut)
{
    errorOut = {};

    auto reader = makeReader(errorOut);
    if (!reader)
        return {};
    if (archive_read_open_memory(reader->get(), bytes.data(), bytes.size()) != ARCHIVE_OK) {
        return fail<TarArchive>(
            errorOut, TarArchiveErrorCode::kOpenFailed,
            formatArchiveDetail(reader->get(), "failed to open tar archive from memory")
        );
    }

    std::map<std::string, std::string, std::less<>> filesOut;
    archive_entry *entry = nullptr;

    while (true) {
        const auto rc = archive_read_next_header(reader->get(), &entry);
        if (rc == ARCHIVE_EOF)
            break;
        if (rc != ARCHIVE_OK) {
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kReadHeaderFailed,
                formatArchiveDetail(reader->get(), "failed to read tar entry header")
            );
        }

        const auto *pathBytes = archive_entry_pathname(entry);
        if (pathBytes == nullptr)
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kMissingPath, "tar entry is missing a pathname"
            );

        const auto path = std::string_view(pathBytes);
        if (!isAllowedPath(path))
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kInvalidPath,
                "tar entry path is not allowed: " + std::string(path)
            );

        if (archive_entry_filetype(entry) != AE_IFREG)
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kUnsupportedEntryType,
                "tar entry is not a regular file: " + std::string(path)
            );
        if (archive_entry_symlink(entry) != nullptr)
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kUnsupportedEntryType,
                "tar entry must not be a symlink: " + std::string(path)
            );
        if (archive_entry_hardlink(entry) != nullptr)
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kUnsupportedEntryType,
                "tar entry must not be a hard link: " + std::string(path)
            );

        auto [it, inserted] = filesOut.emplace(std::string(path), std::string());
        if (!inserted)
            return fail<TarArchive>(
                errorOut, TarArchiveErrorCode::kDuplicateEntry,
                "duplicate tar entry: " + std::string(path)
            );

        auto bytesOut = readEntryBytes(reader->get(), entry, errorOut);
        if (!bytesOut)
            return {};
        it->second = std::move(*bytesOut);
    }

    errorOut = {};
    auto archive = TarArchive(std::move(filesOut));
    return std::optional<TarArchive>(std::move(archive));
}

std::optional<std::string_view> TarArchive::findFile(std::string_view path) const noexcept
{
    const auto it = files.find(path);
    if (it == files.end())
        return {};
    return std::string_view(it->second.data(), it->second.size());
}

} // namespace arkhiv
