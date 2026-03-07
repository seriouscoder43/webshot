#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include <gtest/gtest.h>

#include "arkhiv/tar_archive.hpp"

namespace {

using arkhiv::TarArchive;
using arkhiv::TarArchiveError;
using arkhiv::TarArchiveErrorCode;

using ArchiveWriterPtr = std::unique_ptr<archive, decltype(&archive_write_free)>;

struct TarEntrySpec {
    std::string path;
    std::string body;
    mode_t fileType = AE_IFREG;
    std::string symlinkTarget;
};

int openStringArchive(archive *, void *) { return ARCHIVE_OK; }

la_ssize_t appendArchiveBytes(archive *, void *ctx, const void *buffer, size_t nbytes)
{
    auto &out = *static_cast<std::string *>(ctx);
    out.append(static_cast<const char *>(buffer), nbytes);
    return static_cast<la_ssize_t>(nbytes);
}

int closeStringArchive(archive *, void *) { return ARCHIVE_OK; }

[[noreturn]] void fail(std::string_view message) { throw std::runtime_error(std::string(message)); }

[[nodiscard]] ArchiveWriterPtr makeWriter()
{
    auto *writer = archive_write_new();
    if (writer == nullptr)
        fail("failed to allocate tar writer");
    auto writerPtr = ArchiveWriterPtr(writer, &archive_write_free);

    if (archive_write_set_format_ustar(writerPtr.get()) != ARCHIVE_OK)
        fail("failed to enable ustar output");
    if (archive_write_add_filter_none(writerPtr.get()) != ARCHIVE_OK)
        fail("failed to enable tar output filter");

    return writerPtr;
}

[[nodiscard]] std::string makeTar(const std::vector<TarEntrySpec> &entries)
{
    auto writer = makeWriter();
    std::string out;

    if (archive_write_open(
            writer.get(), &out, &openStringArchive, &appendArchiveBytes, &closeStringArchive
        ) != ARCHIVE_OK)
        fail("failed to open tar writer");

    for (const auto &spec : entries) {
        archive_entry *entry = archive_entry_new();
        if (entry == nullptr)
            fail("failed to allocate tar entry");

        archive_entry_set_pathname(entry, spec.path.c_str());
        archive_entry_set_filetype(entry, spec.fileType);
        archive_entry_set_perm(entry, spec.fileType == AE_IFDIR ? 0755 : 0644);

        if (spec.fileType == AE_IFREG) {
            archive_entry_set_size(entry, static_cast<la_int64_t>(spec.body.size()));
        } else {
            archive_entry_set_size(entry, 0);
        }

        if (!spec.symlinkTarget.empty())
            archive_entry_set_symlink(entry, spec.symlinkTarget.c_str());

        if (archive_write_header(writer.get(), entry) != ARCHIVE_OK) {
            archive_entry_free(entry);
            fail("failed to write tar header");
        }
        if (spec.fileType == AE_IFREG && !spec.body.empty()) {
            if (archive_write_data(writer.get(), spec.body.data(), spec.body.size()) !=
                static_cast<la_ssize_t>(spec.body.size())) {
                archive_entry_free(entry);
                fail("failed to write tar body");
            }
        }

        archive_entry_free(entry);
    }

    if (archive_write_close(writer.get()) != ARCHIVE_OK)
        fail("failed to close tar writer");
    return out;
}

} // namespace

TEST(TarArchive, ReadsRootFilesByName)
{
    const auto tarBytes = makeTar({
        TarEntrySpec{
            .path = "result.json",
            .body = R"({"status":"failed"})",
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
        TarEntrySpec{
            .path = "stdout.log",
            .body = "stdout bytes\n",
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
        TarEntrySpec{
            .path = "stderr.log",
            .body = "stderr bytes\n",
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
        TarEntrySpec{
            .path = "capture.wacz",
            .body = std::string("\x50\x4b\x03\x04", 4),
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
    });

    TarArchiveError error;
    const auto archive = TarArchive::fromBytes(tarBytes, error);

    ASSERT_TRUE(archive);
    EXPECT_EQ(error.code, TarArchiveErrorCode::kNone);

    const auto result = archive->findFile("result.json");
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, std::string_view{R"({"status":"failed"})"});

    const auto stdoutLog = archive->findFile("stdout.log");
    ASSERT_TRUE(stdoutLog);
    EXPECT_EQ(*stdoutLog, std::string_view{"stdout bytes\n"});

    EXPECT_FALSE(archive->findFile("pages.jsonl"));

    const auto wacz = archive->findFile("capture.wacz");
    ASSERT_TRUE(wacz);
    EXPECT_EQ(wacz->size(), static_cast<size_t>(4));
    EXPECT_EQ(static_cast<unsigned char>((*wacz)[0]), 0x50U);
    EXPECT_EQ(static_cast<unsigned char>((*wacz)[1]), 0x4bU);
    EXPECT_EQ(static_cast<unsigned char>((*wacz)[2]), 0x03U);
    EXPECT_EQ(static_cast<unsigned char>((*wacz)[3]), 0x04U);
}

TEST(TarArchive, MissingEntryReturnsEmptyOptional)
{
    const auto tarBytes = makeTar({TarEntrySpec{
        .path = "stdout.log",
        .body = "hello\n",
        .fileType = AE_IFREG,
        .symlinkTarget = "",
    }});
    TarArchiveError error;
    const auto archive = TarArchive::fromBytes(tarBytes, error);

    ASSERT_TRUE(archive);
    EXPECT_EQ(error.code, TarArchiveErrorCode::kNone);
    EXPECT_FALSE(archive->findFile("result.json"));
}

TEST(TarArchive, RejectsDuplicateEntryNames)
{
    const auto tarBytes = makeTar({
        TarEntrySpec{
            .path = "stdout.log",
            .body = "first",
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
        TarEntrySpec{
            .path = "stdout.log",
            .body = "second",
            .fileType = AE_IFREG,
            .symlinkTarget = "",
        },
    });
    TarArchiveError error;

    EXPECT_FALSE(TarArchive::fromBytes(tarBytes, error));
    EXPECT_EQ(error.code, TarArchiveErrorCode::kDuplicateEntry);
}

TEST(TarArchive, RejectsNestedPathEntries)
{
    const auto tarBytes = makeTar({TarEntrySpec{
        .path = "logs/stdout.log",
        .body = "bytes",
        .fileType = AE_IFREG,
        .symlinkTarget = "",
    }});
    TarArchiveError error;

    EXPECT_FALSE(TarArchive::fromBytes(tarBytes, error));
    EXPECT_EQ(error.code, TarArchiveErrorCode::kInvalidPath);
}

TEST(TarArchive, RejectsSymlinkEntries)
{
    const auto tarBytes = makeTar({TarEntrySpec{
        .path = "stdout.log",
        .body = "",
        .fileType = AE_IFLNK,
        .symlinkTarget = "x",
    }});
    TarArchiveError error;

    EXPECT_FALSE(TarArchive::fromBytes(tarBytes, error));
    EXPECT_EQ(error.code, TarArchiveErrorCode::kUnsupportedEntryType);
}

TEST(TarArchive, RejectsInvalidTarBytes)
{
    TarArchiveError error;

    EXPECT_FALSE(TarArchive::fromBytes(std::string_view{"not a tar archive"}, error));
    EXPECT_TRUE(
        error.code == TarArchiveErrorCode::kOpenFailed ||
        error.code == TarArchiveErrorCode::kReadHeaderFailed
    );
}
