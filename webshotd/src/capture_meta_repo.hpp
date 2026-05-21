#pragma once

#include "database.hpp"
#include "expected.hpp"
#include "integers.hpp"
#include "text.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/utils/datetime/timepoint_tz.hpp>

namespace ws {

namespace us = userver;
using Uuid = boost::uuids::uuid;

class CaptureMetaDb;
class Metrics;

struct [[nodiscard]] CaptureMetaIdRow final {
    Uuid uuid;
    us::storages::postgres::TimePointTz created_at;
};

struct [[nodiscard]] CaptureMetaByIdRow final {
    us::storages::postgres::TimePointTz created_at;
    std::string link;
    std::string replay_url;
};

struct [[nodiscard]] InsertCaptureRow final {
    Uuid id;
    us::storages::postgres::TimePointTz created_at;
};

class [[nodiscard]] CaptureMetaRepo final : public us::components::ComponentBase {
public:
    using Base = us::components::ComponentBase;
    static constexpr std::string_view kName = "capture_meta_repo";

    explicit CaptureMetaRepo(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    ~CaptureMetaRepo() override;

    [[nodiscard]] Expected<std::optional<CaptureMetaIdRow>, PgError>
    FindCaptureByLinkHash(const String &link_key, std::string_view content_sha256) const;

    [[nodiscard]] Expected<InsertCaptureRow, PgError> InsertCapture(
        Uuid id, const String &link_key, const String &prefix_key, std::string_view prefix_tree,
        std::string_view content_sha256, const String &replay_url
    ) const;

    [[nodiscard]] Expected<std::optional<CaptureMetaByIdRow>, PgError> FindCapture(Uuid uuid) const;

    [[nodiscard]] Expected<std::vector<CaptureMetaIdRow>, PgError>
    GetCapturesByLinkFirst(const String &link_key, i64 limit) const;
    [[nodiscard]] Expected<std::vector<CaptureMetaIdRow>, PgError> GetCapturesByLinkNext(
        const String &link_key, i64 limit, const us::storages::postgres::TimePointTz &created_at,
        Uuid id
    ) const;
    [[nodiscard]] Expected<std::vector<CaptureMetaIdRow>, PgError> GetCapturesByLinkPrev(
        const String &link_key, i64 limit, const us::storages::postgres::TimePointTz &created_at,
        Uuid id
    ) const;

    [[nodiscard]] Expected<std::vector<String>, PgError> GetDistinctLinksByPrefixFirst(
        const String &prefix_inclusive, const std::string &upper_exclusive, i64 limit
    ) const;
    [[nodiscard]] Expected<std::vector<String>, PgError> GetDistinctLinksByPrefixNext(
        const String &prefix_inclusive, const std::string &upper_exclusive, const String &from_link,
        i64 limit
    ) const;
    [[nodiscard]] Expected<std::vector<String>, PgError> GetDistinctLinksByPrefixPrev(
        const String &prefix_inclusive, const std::string &upper_exclusive, const String &from_link,
        i64 limit
    ) const;

    [[nodiscard]] Expected<bool, PgError> HasRowsBeforeInLink(
        const String &link_key, const us::storages::postgres::TimePointTz &created_at, Uuid id
    ) const;

    [[nodiscard]] Expected<std::vector<Uuid>, PgError>
    GetCaptureIdsByPrefixTree(std::string_view prefix_tree, i64 limit) const;

    [[nodiscard]] Expected<void, PgError> DeleteCapturesByIds(const std::vector<Uuid> &ids) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ws
