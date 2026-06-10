#pragma once

#include "crawler/artifacts.hpp"
#include "crawler/fallback.hpp"
#include "crawler/limits.hpp"
#include "integers.hpp"

#include "chrono.hpp"
#include <optional>
#include <string>

#include <userver/clients/dns/resolver_fwd.hpp>
#include <userver/engine/subprocess/process_starter.hpp>
#include <userver/engine/task/task_processor_fwd.hpp>

namespace ws {
namespace us = userver;
namespace eng = us::engine;
class AccessPolicyStore;
class Config;
class Metrics;
namespace crawler {

struct [[nodiscard]] CaptureTimings {
    ws::chrono::seconds post_load_delay;
    ws::chrono::seconds net_idle_wait;
    ws::chrono::seconds page_extra_delay;
    ws::chrono::seconds behavior_timeout;
};

struct [[nodiscard]] CrawlerTunables {
    ws::chrono::seconds devtools_startup_timeout;
    ws::chrono::seconds cdp_handshake_timeout;
    ws::chrono::seconds cdp_command_timeout;
    ws::chrono::milliseconds devtools_poll_interval;
    ws::chrono::milliseconds browser_stop_timeout;
    ws::chrono::milliseconds proxy_stop_timeout;
    bool enable_local_fixture_rewrite;
};

} // namespace crawler

struct [[nodiscard]] CrawlerRunArtifacts {
    std::optional<crawler::CrawlerError> error;
    std::string stdout_log;
    std::string stderr_log;
    std::optional<std::string> wacz;
    std::optional<std::string> pages_jsonl;
    std::optional<std::string> content_sha256;
    std::optional<String> replay_url;
};

class [[nodiscard]] CrawlerRunner final {
public:
    CrawlerRunner(
        AccessPolicyStore &access_policy, const Config &config,
        us::clients::dns::Resolver &dns_resolver, eng::subprocess::ProcessStarter &process_starter,
        ws::chrono::seconds run_timeout, eng::TaskProcessor &fs_task_processor,
        std::string state_dir, std::optional<crawler::CgroupLimits> limits, i64 max_archive_bytes,
        crawler::CaptureTimings timings, crawler::CrawlerTunables tunables,
        i64 network_down_bytes_ratio_max, Metrics &metrics
    );

    [[nodiscard]] CrawlerRunArtifacts Run(const String &seed_url) const;

private:
    AccessPolicyStore &access_policy_;
    const Config &config_;
    us::clients::dns::Resolver &dns_resolver_;
    eng::subprocess::ProcessStarter &process_starter_;
    eng::TaskProcessor &fs_task_processor_;
    ws::chrono::seconds run_timeout_;
    std::string browser_runs_root_;
    std::string cgroup_root_path_;
    std::optional<crawler::CgroupLimits> cgroup_limits_;
    i64 max_archive_bytes_;
    crawler::CaptureTimings timings_;
    crawler::CrawlerTunables tunables_;
    i64 network_down_bytes_ratio_max_;
    Metrics &metrics_;
};

} // namespace ws
