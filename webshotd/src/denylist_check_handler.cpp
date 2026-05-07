#include "denylist_check_handler.hpp"
/**
 * @file
 * @brief Internal endpoint for checking whether a link is denylisted.
 */
#include "access_policy.hpp"
#include "config.hpp"
#include "handler_request_support.hpp"
#include "http_utils.hpp"
#include "metrics.hpp"
#include "prefix_utils.hpp"

#include <chrono>
#include <string>

#include <userver/components/component.hpp>
#include <userver/engine/exception.hpp>
#include <userver/engine/task/current_task.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace ws {
namespace us = userver;
namespace server = us::server;
using namespace std::chrono_literals;

AccessPolicyCheckHandler::AccessPolicyCheckHandler(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : HttpHandlerBase(config, context), config_(context.FindComponent<Config>()),
      access_policy_(context.FindComponent<AccessPolicyStore>()),
      metrics_(context.FindComponent<Metrics>()), crud_(context.FindComponent<Crud>()),
      request_timeout_(config["request-timeout-ms"].As<int64_t>() * 1ms)
{
}

us::yaml_config::Schema AccessPolicyCheckHandler::GetStaticConfigSchema()
{
    return us::yaml_config::MergeSchemas<server::handlers::HttpHandlerBase>(R"(
type: object
description: Access policy check handler static config
additionalProperties: false
properties:
  request-timeout-ms:
    type: integer
    minimum: 1
    description: Upper bound for /ws/denylist/check handler in milliseconds
)");
}

std::string AccessPolicyCheckHandler::HandleRequestThrow(
    const server::http::HttpRequest &request, server::request::RequestContext &
) const
{
    using enum server::http::HttpStatus;

    auto &response = request.GetHttpResponse();
    HandlerRequestSupport request_support{crud_, config_};
    request_support.ApplyRequestDeadline(request, request_timeout_);
    const auto link = ParseJsonLinkBody(request, config_);
    if (!link)
        return httpu::RespondError(response, kBadRequest, link.Error());

    auto prefix_key = prefix::MakePrefixKey(*link);
    const auto allowed = access_policy_.IsAllowedPrefix(prefix_key);
    if (!allowed) {
        metrics_.AccountError(Metrics::Error::kAccessPolicyCheck);
        response.SetStatus(kInternalServerError);
        return {};
    }
    if (!*allowed) {
        response.SetStatus(kForbidden);
        return {};
    }
    response.SetStatus(kNoContent);
    return {};
}

} // namespace ws
