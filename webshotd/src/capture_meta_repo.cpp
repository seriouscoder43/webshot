#include "capture_meta_repo.hpp"

#include "integers_postgres_formatter.hpp" // IWYU pragma: keep
#include "metrics.hpp"
#include "postgres_db_component.hpp"
#include "text_postgres_formatter.hpp" // IWYU pragma: keep
#include "try.hpp"

#include <webshot/sql_queries.hpp>

#include <utility>

#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/bytea.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace ws {

namespace us = userver;
namespace pg = us::storages::postgres;
namespace sql = webshot::sql;

struct CaptureMetaRepo::Impl final {
    explicit Impl(const us::components::ComponentContext &context)
        : db(context.FindComponent<PostgresDbComponent>("capture_meta_db")),
          metrics(context.FindComponent<Metrics>())
    {
    }

    template <typename F, typename... Ts> [[nodiscard]] auto RunReadonly(F &&f, Ts &&...args) const
    {
        auto out = db.RunReadonly(std::forward<F>(f), std::forward<Ts>(args)...);
        if (!out)
            metrics.AccountError(Metrics::Error::kDbCaptureMetaRead);
        return out;
    }

    template <typename F, typename... Ts> [[nodiscard]] auto RunReadwrite(F &&f, Ts &&...args) const
    {
        auto out = db.RunReadwrite(std::forward<F>(f), std::forward<Ts>(args)...);
        if (!out)
            metrics.AccountError(Metrics::Error::kDbCaptureMetaWrite);
        return out;
    }

    PostgresDbComponent &db;
    Metrics &metrics;
};

CaptureMetaRepo::CaptureMetaRepo(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : Base(config, context), impl_(std::make_unique<Impl>(context))
{
}

CaptureMetaRepo::~CaptureMetaRepo() = default;

Expected<std::optional<CaptureMetaIdRow>, PgError> CaptureMetaRepo::FindCaptureByLinkHash(
    const String &link_key, std::string_view content_sha256
) const
{
    struct Row {
        Uuid uuid;
        pg::TimePointTz created_at;
    };
    auto opt = TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsOptionalSingleRow<Row>(pg::kRowTag); },
        sql::kSelectCaptureByLinkHash, link_key, pg::Bytea(content_sha256)
    ));
    auto row = TRY(opt);
    return CaptureMetaIdRow{.uuid = row.uuid, .created_at = row.created_at};
}

Expected<InsertCaptureRow, PgError> CaptureMetaRepo::InsertCapture(
    Uuid id, const String &link_key, const String &prefix_key, std::string_view prefix_tree,
    std::string_view content_sha256, const String &replay_url
) const
{
    struct Row {
        Uuid id;
        pg::TimePointTz created_at;
    };
    auto row = TRY(impl_->RunReadwrite(
        [&](auto &res) { return res.template AsSingleRow<Row>(pg::kRowTag); }, sql::kInsertCapture,
        id, link_key, prefix_key, prefix_tree, pg::Bytea(content_sha256), replay_url
    ));
    return InsertCaptureRow{.id = row.id, .created_at = row.created_at};
}

Expected<std::optional<CaptureMetaByIdRow>, PgError> CaptureMetaRepo::FindCapture(Uuid uuid) const
{
    struct Row {
        pg::TimePointTz created_at;
        std::string link;
        std::string replay_url;
    };
    auto opt = TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsOptionalSingleRow<Row>(pg::kRowTag); },
        sql::kSelectCapture, uuid
    ));
    auto row = TRY(opt);
    return CaptureMetaByIdRow{
        .created_at = row.created_at,
        .link = std::move(row.link),
        .replay_url = std::move(row.replay_url),
    };
}

Expected<std::vector<CaptureMetaIdRow>, PgError>
CaptureMetaRepo::GetCapturesByLinkFirst(const String &link_key, i64 limit) const
{
    struct Row {
        Uuid uuid;
        pg::TimePointTz created_at;
    };
    auto rows = TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<Row>>(pg::kRowTag); },
        sql::kSelectCaptureByLinkFirst, link_key, limit
    ));
    std::vector<CaptureMetaIdRow> out;
    out.reserve(rows.size());
    for (auto &&row : rows)
        out.emplace_back(row.uuid, row.created_at);
    return out;
}

Expected<std::vector<CaptureMetaIdRow>, PgError> CaptureMetaRepo::GetCapturesByLinkNext(
    const String &link_key, i64 limit, const pg::TimePointTz &created_at, Uuid id
) const
{
    struct Row {
        Uuid uuid;
        pg::TimePointTz created_at;
    };
    auto rows = TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<Row>>(pg::kRowTag); },
        sql::kSelectCaptureByLinkNext, link_key, limit, created_at, id
    ));
    std::vector<CaptureMetaIdRow> out;
    out.reserve(rows.size());
    for (auto &&row : rows)
        out.emplace_back(row.uuid, row.created_at);
    return out;
}

Expected<std::vector<CaptureMetaIdRow>, PgError> CaptureMetaRepo::GetCapturesByLinkPrev(
    const String &link_key, i64 limit, const pg::TimePointTz &created_at, Uuid id
) const
{
    struct Row {
        Uuid uuid;
        pg::TimePointTz created_at;
    };
    auto rows = TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<Row>>(pg::kRowTag); },
        sql::kSelectCaptureByLinkPrev, link_key, limit, created_at, id
    ));
    std::vector<CaptureMetaIdRow> out;
    out.reserve(rows.size());
    for (auto &&row : rows)
        out.emplace_back(row.uuid, row.created_at);
    return out;
}

Expected<std::vector<String>, PgError> CaptureMetaRepo::GetDistinctLinksByPrefixFirst(
    const String &prefix_inclusive, const std::string &upper_exclusive, i64 limit
) const
{
    return TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<String>>(); },
        sql::kSelectDistinctLinksByPrefixFirst, prefix_inclusive, upper_exclusive, limit
    ));
}

Expected<std::vector<String>, PgError> CaptureMetaRepo::GetDistinctLinksByPrefixNext(
    const String &prefix_inclusive, const std::string &upper_exclusive, const String &from_link,
    i64 limit
) const
{
    return TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<String>>(); },
        sql::kSelectDistinctLinksByPrefixNext, prefix_inclusive, upper_exclusive, from_link, limit
    ));
}

Expected<std::vector<String>, PgError> CaptureMetaRepo::GetDistinctLinksByPrefixPrev(
    const String &prefix_inclusive, const std::string &upper_exclusive, const String &from_link,
    i64 limit
) const
{
    return TRY(impl_->RunReadonly(
        [&](auto &res) { return res.template AsContainer<std::vector<String>>(); },
        sql::kSelectDistinctLinksByPrefixPrev, prefix_inclusive, upper_exclusive, from_link, limit
    ));
}

Expected<bool, PgError> CaptureMetaRepo::HasRowsBeforeInLink(
    const String &link_key, const pg::TimePointTz &created_at, Uuid id
) const
{
    struct Row {
        Uuid uuid;
        pg::TimePointTz created_at;
    };
    return !TRY(impl_->RunReadonly(
                    [&](auto &res) {
                        return res.template AsContainer<std::vector<Row>>(pg::kRowTag);
                    },
                    sql::kSelectCaptureByLinkPrev, link_key, 1, created_at, id
                ))
                .empty();
}

Expected<std::vector<Uuid>, PgError>
CaptureMetaRepo::GetCaptureIdsByPrefixTree(std::string_view prefix_tree, i64 limit) const
{
    return TRY(impl_->RunReadonly(
        [&](auto &res) {
            std::vector<Uuid> ids_out;
            ids_out.reserve(res.Size());
            for (auto row : res)
                ids_out.emplace_back(row[0].template As<Uuid>());
            return ids_out;
        },
        sql::kSelectIdsByDenyPrefixPaged, prefix_tree, limit
    ));
}

Expected<void, PgError> CaptureMetaRepo::DeleteCapturesByIds(const std::vector<Uuid> &ids) const
{
    TRY(impl_->RunReadwrite([](auto &) {}, sql::kDeleteCapturesByIds, ids));
    return {};
}

} // namespace ws
