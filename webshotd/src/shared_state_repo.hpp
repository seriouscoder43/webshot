#pragma once

#include "database.hpp"
#include "expected.hpp"
#include "text.hpp"

#include "chrono.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/utils/datetime/timepoint_tz.hpp>

namespace ws {

namespace us = userver;
namespace datetime = us::utils::datetime;
using Uuid = boost::uuids::uuid;

class Metrics;
class SharedStateDb;

struct [[nodiscard]] SharedCrawlJobRow final {
    Uuid uuid;
    String link;
    std::string status;
    std::optional<std::string> error_category;
    std::optional<std::string> error_message;
    us::storages::postgres::TimePointTz created_at;
    std::optional<us::storages::postgres::TimePointTz> started_at;
    std::optional<us::storages::postgres::TimePointTz> finished_at;
    std::optional<us::storages::postgres::TimePointTz> result_created_at;
    std::optional<Uuid> result_capture_id;
};

struct [[nodiscard]] MakeCrawlJobRow final {
    us::storages::postgres::TimePointTz created_at;
};

struct [[nodiscard]] JobDurationRow final {
    Uuid id;
    int64_t duration_ms;
};

class [[nodiscard]] SharedStateRepo final : public us::components::ComponentBase {
public:
    using Base = us::components::ComponentBase;
    static constexpr std::string_view kName = "shared_state_repo";

    explicit SharedStateRepo(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    ~SharedStateRepo() override;

    [[nodiscard]] Expected<bool, PgError> CheckDenylistTree(std::string_view prefix_tree) const;
    [[nodiscard]] Expected<bool, PgError> CheckAllowlistTree(std::string_view prefix_tree) const;

    [[nodiscard]] Expected<void, PgError> InsertDenylistPrefix(
        const String &prefix_key, std::string_view prefix_tree, const String &reason
    ) const;

    [[nodiscard]] Expected<void, PgError> InsertAllowlistPrefix(
        const String &prefix_key, std::string_view prefix_tree, const String &reason
    ) const;

    [[nodiscard]] Expected<void, PgError> DeleteAllowlistPrefix(const String &prefix_key) const;

    [[nodiscard]] Expected<datetime::TimePointTz, PgError>
    InsertCrawlJob(Uuid id, const String &link_key) const;

    [[nodiscard]] Expected<void, PgError> MarkCrawlJobRunning(Uuid id) const;
    [[nodiscard]] Expected<ws::chrono::milliseconds, PgError> MarkCrawlJobSucceeded(
        Uuid id, Uuid result_capture_id, const datetime::TimePointTz &created_at
    ) const;
    [[nodiscard]] Expected<ws::chrono::milliseconds, PgError>
    MarkCrawlJobFailed(Uuid id, const String &error_category, const String &error_message) const;

    [[nodiscard]] Expected<std::optional<SharedCrawlJobRow>, PgError> LoadCrawlJob(Uuid id) const;

    [[nodiscard]] Expected<std::optional<SharedCrawlJobRow>, PgError>
    FindLatestCrawlJobForLink(const String &link_key) const;

    struct [[nodiscard]] MakeOrReuseCrawlJobResult final {
        SharedCrawlJobRow job;
        bool created{false};
    };
    [[nodiscard]] Expected<MakeOrReuseCrawlJobResult, PgError>
    MakeOrReuseCrawlJobLocked(const String &link_key, ws::chrono::seconds link_ratelimit) const;

    [[nodiscard]] Expected<void, PgError>
    DeleteCrawlJobsExpired(const datetime::TimePointTz &cutoff) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ws
