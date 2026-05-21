#include "shared_state_repo.hpp"

#include <webshot/sql_queries.hpp>

#include "grab_value.hpp"
#include "metrics.hpp"
#include "postgres_db_component.hpp"
#include "text_postgres_formatter.hpp"
#include "try.hpp"

#include <chrono>
#include <format>
#include <utility>

#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace ws {

namespace us = userver;
namespace pg = us::storages::postgres;
namespace datetime = us::utils::datetime;
namespace chrono = std::chrono;
namespace sql = webshot::sql;

using namespace std::chrono_literals;

struct SharedStateRepo::Impl final {
    explicit Impl(const us::components::ComponentContext &context)
        : db(context.FindComponent<PostgresDbComponent>("shared_state_db")),
          metrics(context.FindComponent<Metrics>())
    {
    }

    template <typename F, typename... Ts> [[nodiscard]] auto Readonly(F &&f, Ts &&...args) const
    {
        auto out = db.RunReadonly(std::forward<F>(f), std::forward<Ts>(args)...);
        if (!out)
            metrics.AccountError(Metrics::Error::kDbSharedStateRead);
        return out;
    }

    template <typename F, typename... Ts> [[nodiscard]] auto Readwrite(F &&f, Ts &&...args) const
    {
        auto out = db.RunReadwrite(std::forward<F>(f), std::forward<Ts>(args)...);
        if (!out)
            metrics.AccountError(Metrics::Error::kDbSharedStateWrite);
        return out;
    }

    template <typename F> [[nodiscard]] auto ReadwriteTransaction(F &&f) const
    {
        auto out = db.RunReadwriteTransaction(std::forward<F>(f));
        if (!out)
            metrics.AccountError(Metrics::Error::kDbSharedStateWrite);
        return out;
    }

    PostgresDbComponent &db;
    Metrics &metrics;
};

SharedStateRepo::SharedStateRepo(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : Base(config, context), impl_(std::make_unique<Impl>(context))
{
}

SharedStateRepo::~SharedStateRepo() = default;

Expected<bool, PgError> SharedStateRepo::CheckDenylistTree(std::string_view prefix_tree) const
{
    auto denied = TRY(impl_->Readonly(
        [&](auto &res) { return res.template AsSingleRow<bool>(); }, sql::kCheckDenylistTree,
        prefix_tree
    ));
    return denied;
}

Expected<bool, PgError> SharedStateRepo::CheckAllowlistTree(std::string_view prefix_tree) const
{
    auto allowlisted = TRY(impl_->Readonly(
        [&](auto &res) { return res.template AsSingleRow<bool>(); }, sql::kCheckAllowlistTree,
        prefix_tree
    ));
    return allowlisted;
}

Expected<void, PgError> SharedStateRepo::InsertDenylistPrefix(
    const String &prefix_key, std::string_view prefix_tree, const String &reason
) const
{
    return impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kInsertDenylistPrefix, prefix_key,
        prefix_tree, reason
    );
}

Expected<void, PgError> SharedStateRepo::InsertAllowlistPrefix(
    const String &prefix_key, std::string_view prefix_tree, const String &reason
) const
{
    return impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kInsertAllowlistPrefix, prefix_key,
        prefix_tree, reason
    );
}

Expected<void, PgError> SharedStateRepo::DeleteAllowlistPrefix(const String &prefix_key) const
{
    return impl_->Readwrite(
        [&](auto &res) { static_cast<void>(res); }, sql::kDeleteAllowlistPrefix, prefix_key
    );
}

Expected<datetime::TimePointTz, PgError>
SharedStateRepo::InsertCrawlJob(Uuid id, const String &link_key) const
{
    struct Row {
        pg::TimePointTz created_at;
    };
    auto row = TRY(impl_->Readwrite(
        [&](auto &res) { return res.template AsSingleRow<Row>(pg::kRowTag); }, sql::kInsertCrawlJob,
        id, link_key
    ));
    return datetime::TimePointTz(row.created_at.GetUnderlying());
}

Expected<void, PgError> SharedStateRepo::MarkCrawlJobRunning(Uuid id) const
{
    return impl_->Readwrite([](auto &) {}, sql::kUpdateCrawlJobRunning, id);
}

Expected<chrono::milliseconds, PgError> SharedStateRepo::MarkCrawlJobSucceeded(
    Uuid id, Uuid result_capture_id, const datetime::TimePointTz &created_at
) const
{
    struct Row {
        Uuid id;
        int64_t duration_ms;
    };
    auto row = TRY(impl_->Readwrite(
        [&](auto &res) { return res.template AsSingleRow<Row>(pg::kRowTag); },
        sql::kUpdateCrawlJobSucceeded, id, pg::TimePointTz{created_at.GetTimePoint()},
        result_capture_id
    ));
    return row.duration_ms * 1ms;
}

Expected<chrono::milliseconds, PgError> SharedStateRepo::MarkCrawlJobFailed(
    Uuid id, const String &error_category, const String &error_message
) const
{
    struct Row {
        Uuid id;
        int64_t duration_ms;
    };
    auto row = TRY(impl_->Readwrite(
        [&](auto &res) { return res.template AsSingleRow<Row>(pg::kRowTag); },
        sql::kUpdateCrawlJobFailed, id, error_category, error_message
    ));
    return row.duration_ms * 1ms;
}

Expected<std::optional<SharedCrawlJobRow>, PgError> SharedStateRepo::LoadCrawlJob(Uuid id) const
{
    auto row_opt = TRY(impl_->Readonly(
        [&](auto &res) { return res.template AsOptionalSingleRow<SharedCrawlJobRow>(pg::kRowTag); },
        sql::kSelectCrawlJob, id
    ));
    auto row = TRY(row_opt);
    return std::move(row);
}

Expected<std::optional<SharedCrawlJobRow>, PgError>
SharedStateRepo::FindLatestCrawlJobForLink(const String &link_key) const
{
    auto row_opt = TRY(impl_->Readonly(
        [&](auto &res) { return res.template AsOptionalSingleRow<SharedCrawlJobRow>(pg::kRowTag); },
        sql::kSelectLatestCrawlJobByLink, link_key
    ));
    auto row = TRY(row_opt);
    return std::move(row);
}

Expected<SharedStateRepo::MakeOrReuseCrawlJobResult, PgError>
SharedStateRepo::MakeOrReuseCrawlJobLocked(
    const String &link_key, chrono::seconds link_ratelimit
) const
{
    struct Row {
        pg::TimePointTz created_at;
    };

    return impl_->ReadwriteTransaction([&](auto &trx) -> MakeOrReuseCrawlJobResult {
        if (link_ratelimit > 0s) {
            trx.Execute(sql::kLockCrawlJobLink, text::Format("link:{}", link_key));
        }

        if (link_ratelimit > 0s) {
            auto latest_job_row_opt = trx.Execute(sql::kSelectLatestCrawlJobByLink, link_key)
                                          .template AsOptionalSingleRow<SharedCrawlJobRow>(
                                              pg::kRowTag
                                          );
            if (latest_job_row_opt) {
                auto job = GrabValueOf(latest_job_row_opt);
                auto now = datetime::Now();
                auto last_created = datetime::TimePointTz(job.created_at.GetUnderlying());
                auto ratelimit_until = last_created.GetTimePoint() + link_ratelimit;
                if (now < ratelimit_until) {
                    trx.Commit();
                    return SharedStateRepo::MakeOrReuseCrawlJobResult{
                        .job = std::move(job),
                        .created = false,
                    };
                }
            }
        }

        auto id = us::utils::generators::GenerateBoostUuid();
        auto row =
            trx.Execute(sql::kInsertCrawlJob, id, link_key).template AsSingleRow<Row>(pg::kRowTag);
        trx.Commit();

        SharedCrawlJobRow out{};
        out.uuid = id;
        out.link = link_key;
        out.status = "pending";
        out.created_at = row.created_at;

        return MakeOrReuseCrawlJobResult{.job = std::move(out), .created = true};
    });
}

Expected<void, PgError>
SharedStateRepo::DeleteCrawlJobsExpired(const datetime::TimePointTz &cutoff) const
{
    TRY(impl_->Readwrite([](auto &) {}, sql::kDeleteCrawlJobsExpired, pg::TimePointTz{cutoff}));
    return {};
}

} // namespace ws
