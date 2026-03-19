#include "arkhiv/gzip.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arkhiv {

namespace {

using ArchiveWriterPtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
using ArchiveEntryPtr = std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;

[[nodiscard]] std::string formatArchiveDetail(archive *handle, std::string_view context)
{
    auto detail = std::string(context);
    const auto *archiveDetail = archive_error_string(handle);
    if (archiveDetail != nullptr) {
        detail += ": ";
        detail += archiveDetail;
    }
    return detail;
}

int openStringArchive(archive *, void *) { return ARCHIVE_OK; }

la_ssize_t appendArchiveBytes(archive *, void *ctx, const void *buffer, size_t nbytes)
{
    auto &out = *static_cast<std::string *>(ctx);
    out.append(static_cast<const char *>(buffer), nbytes);
    return static_cast<la_ssize_t>(nbytes);
}

int closeStringArchive(archive *, void *) { return ARCHIVE_OK; }

[[nodiscard]] ArchiveEntryPtr makeEntry(size_t size)
{
    auto entry = ArchiveEntryPtr(archive_entry_new(), &archive_entry_free);
    if (!entry)
        return {nullptr, &archive_entry_free};

    archive_entry_set_pathname(entry.get(), "member");
    archive_entry_set_filetype(entry.get(), AE_IFREG);
    archive_entry_set_perm(entry.get(), 0644);
    archive_entry_set_size(entry.get(), static_cast<la_int64_t>(size));
    return entry;
}

} // namespace

std::optional<std::string> gzipCompressMember(std::string_view body, GzipError &errorOut)
{
    errorOut = {};

    auto *writer = archive_write_new();
    if (writer == nullptr) {
        errorOut.code = GzipErrorCode::kWriterInitFailed;
        errorOut.detail = "failed to allocate gzip writer";
        return {};
    }

    auto writerPtr = ArchiveWriterPtr(writer, &archive_write_free);

    if (archive_write_set_format_raw(writerPtr.get()) != ARCHIVE_OK) {
        errorOut.code = GzipErrorCode::kWriterInitFailed;
        errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to enable raw output");
        return {};
    }
    if (archive_write_add_filter_gzip(writerPtr.get()) != ARCHIVE_OK) {
        errorOut.code = GzipErrorCode::kWriterInitFailed;
        errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to enable gzip filter");
        return {};
    }

    std::string out;
    if (archive_write_open(
            writerPtr.get(), &out, &openStringArchive, &appendArchiveBytes, &closeStringArchive
        ) != ARCHIVE_OK) {
        errorOut.code = GzipErrorCode::kOpenFailed;
        errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to open gzip writer");
        return {};
    }

    auto entry = makeEntry(body.size());
    if (!entry) {
        errorOut.code = GzipErrorCode::kWriterInitFailed;
        errorOut.detail = "failed to allocate gzip entry";
        return {};
    }

    if (archive_write_header(writerPtr.get(), entry.get()) != ARCHIVE_OK) {
        errorOut.code = GzipErrorCode::kWriteHeaderFailed;
        errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to write gzip header");
        return {};
    }

    if (!body.empty()) {
        const auto written = archive_write_data(
            writerPtr.get(), body.data(), static_cast<size_t>(body.size())
        );
        if (written != static_cast<la_ssize_t>(body.size())) {
            errorOut.code = GzipErrorCode::kWriteDataFailed;
            errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to write gzip body");
            return {};
        }
    }

    if (archive_write_close(writerPtr.get()) != ARCHIVE_OK) {
        errorOut.code = GzipErrorCode::kFinishFailed;
        errorOut.detail = formatArchiveDetail(writerPtr.get(), "failed to close gzip writer");
        return {};
    }

    errorOut = {};
    return out;
}

} // namespace arkhiv
