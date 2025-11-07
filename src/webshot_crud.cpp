#include "include/webshot_crud.hpp"
#include "include/sql.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include <boost/uuid/uuid.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_base.hpp>
#include <userver/concurrent/background_task_storage.hpp>
#include <userver/engine/subprocess/process_starter.hpp>
#include <userver/engine/task/current_task.hpp>
#include <userver/engine/task/task_processor_fwd.hpp>
#include <userver/fs/blocking/temp_directory.hpp>
#include <userver/fs/blocking/write.hpp>
#include <userver/fs/read.hpp>
#include <userver/fs/write.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/storages/postgres/postgres.hpp>
#include <userver/utils/async.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/yaml_config.hpp>

using namespace v1;
namespace pg = us::storages::postgres;
namespace engine = us::engine;
namespace concurrent = userver::concurrent;
namespace utils = us::utils;

us::yaml_config::Schema WebshotCrud::GetStaticConfigSchema()
{
    return us::yaml_config::MergeSchemas<us::components::ComponentBase>(R"(
type: object
description: '.'
additionalProperties: false
properties:
    webshot-root:
        type: string
        description: '.'
    webshots-page-max:
        type: integer
        description: '.'
    webshot-storage-url:
        type: string
        description: '.'
)");
}

WebshotCrud::WebshotCrud(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : us::components::ComponentBase(config, context),
      impl(std::make_unique<WebshotCrud::Impl>(
          config["webshot-root"].As<std::string>(), config["webshots-page-max"].As<ssize_t>(),
          config["webshot-storage-url"].As<std::string>(),
          context.FindComponent<us::components::Postgres>("webshot-db").GetCluster(),
          engine::current_task::GetTaskProcessor()
      ))
{
}

WebshotCrud::~WebshotCrud() = default;

class [[nodiscard]] WebshotCrud::Impl {
public:
    const std::string webshotRoot;
    const ssize_t webshotsPageMax;
    const std::string webshotStorageUrl;
    pg::ClusterPtr cluster;
    // must die first
    concurrent::BackgroundTaskStorage backgroundTaskStorage;
    Impl(
        std::string webshotRoot_, ssize_t webshotsPageMax_, std::string webshotStorageUrl_,
        pg::ClusterPtr cluster_, engine::TaskProcessor &tp_
    )
        : webshotRoot(webshotRoot_), webshotsPageMax(webshotsPageMax_),
          webshotStorageUrl(webshotStorageUrl_), cluster(std::move(cluster_)),
          backgroundTaskStorage(tp_)
    {
    }
    template <typename... Ts> [[nodiscard]] auto readonly(Ts &&...args)
    {
        return cluster->Execute(pg::ClusterHostType::kSlaveOrMaster, std::forward<Ts>(args)...);
    }

    template <typename... Ts> [[nodiscard]] auto readwrite(Ts &&...args)
    {
        return cluster->Execute(pg::ClusterHostType::kMaster, std::forward<Ts>(args)...);
    }
};

void WebshotCrud::createWebshot(std::string url)
{
    impl->backgroundTaskStorage.AsyncDetach("create-webshot-lambda", [this, url]() -> void {
        engine::subprocess::ProcessStarter starter(engine::current_task::GetBlockingTaskProcessor()
        );
        const std::string kWaczName = "1";
        auto archiveRoot = us::fs::blocking::TempDirectory::Create();
        engine::subprocess::ExecOptions exec_opts;
        exec_opts.use_path = true;

        auto child = starter.Exec(
            "docker",
            {"run",
             "--rm",
             "-v",
             fmt::format("{}:/crawls", archiveRoot.GetPath()),
             "--network",
             "host",
             "--shm-size",
             "1g",
             "webrecorder/browsertrix-crawler",
             "crawl",
             "--collection",
             kWaczName,
             "--generateWACZ",
             "--workers",
             "1",
             "--headless",
             "--scopeType",
             "page-spa",
             "--pageLimit",
             "1",
             "--pageLoadTimeout",
             "10",
             "--postLoadDelay",
             "1",
             "--netIdleWait",
             "0",
             "--pageExtraDelay",
             "3",
             "--behaviorTimeout",
             "1",
             "--waitUntil",
             "load",
             "--blockAds",
             "--behaviors",
             "",
             "--lang",
             "en",
             "--context",
             "general,worker,pageStatus,writer,storage,jsError,state,crawlStatus,fetch,wacz",
             "--logLevel",
             "debug,info",
             "--logging",
             "debug,stats,jserrors",
             "--logExcludeContext",
             "",
             "--url",
             url},
            std::move(exec_opts)
        );
        auto status = child.Get();

        if (!status.IsExited() || status.GetExitCode() != 0) {
            LOG_INFO() << fmt::format("Failed to crawl {}, child process failed", url);
            return;
        }
        const auto pathToWaczFile = fmt::format(
            "{0}/collections/{1}/{1}.wacz", archiveRoot.GetPath(), kWaczName
        );
        if (!us::fs::FileExists(engine::current_task::GetBlockingTaskProcessor(), pathToWaczFile)) {
            LOG_INFO() << fmt::format("Failed to crawl {}, no WACZ", url);
            return;
        }
        const auto uuid =
            impl->readwrite(sql::kInsertWebshot.data(), url).AsSingleRow<boost::uuids::uuid>();
        us::fs::CreateDirectories(
            engine::current_task::GetBlockingTaskProcessor(), impl->webshotRoot
        );
        us::fs::blocking::Rename(
            pathToWaczFile, fmt::format("{}/{}", impl->webshotRoot, utils::ToString(uuid))
        );
    });
}

std::optional<Webshot> WebshotCrud::findWebshot(boost::uuids::uuid uuid)
{
    const auto location =
        impl->readonly(sql::kSelectWebshot.data(), uuid).AsOptionalSingleRow<boost::uuids::uuid>();
    if (!location) {
        LOG_INFO() << fmt::format("UUID not found: {}", us::utils::ToString(uuid));
        return {};
    }
    return {{fmt::format("{}/{}", impl->webshotStorageUrl, utils::ToString(*location))}};
}

std::vector<std::string> WebshotCrud::findWebshotByUrl(const std::string &url)
{
    const auto ids = impl->readonly(sql::kSelectWebshotByUrl.data(), url, impl->webshotsPageMax)
                         .AsContainer<std::vector<boost::uuids::uuid>>();
    std::vector<std::string> uuids;
    std::transform(begin(ids), end(ids), std::back_inserter(uuids), [](auto id) {
        return utils::ToString(id);
    });
    return uuids;
}
