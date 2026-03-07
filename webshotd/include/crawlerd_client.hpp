#pragma once

#include "crawler_fallback.hpp"
#include "text.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <userver/clients/http/client.hpp>

namespace us = userver;

namespace v1 {

struct [[nodiscard]] CrawlerRunArtifacts {
    crawler::AttemptSummary attempt;
    std::string stdoutLog;
    std::string stderrLog;
    std::optional<std::string> wacz;
    std::optional<std::string> pagesJsonl;
};

class [[nodiscard]] CrawlerdClient final {
public:
    CrawlerdClient(
        us::clients::http::Client &httpClient, String baseUrl, String socketPath,
        int64_t runTimeoutSec
    );

    [[nodiscard]] CrawlerRunArtifacts run(const String &seedUrl) const;

private:
    us::clients::http::Client &httpClient;
    String baseUrl;
    String socketPath;
    int64_t runTimeoutSec;
};

} // namespace v1
