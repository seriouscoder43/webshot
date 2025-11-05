#include <ada.h>

#include <userver/components/component.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_response_body_stream.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>
#include <userver/yaml_config/yaml_config.hpp>

#include "include/url_validation.hpp"
#include "include/webshot_handler.hpp"

using namespace v1;

WebshotHandler::WebshotHandler(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : HttpHandlerBase(config, context), crud(context.FindComponent<WebshotCrud>())
{
}

std::string WebshotHandler::
    HandleRequestThrow(const server::http::HttpRequest &request, server::request::RequestContext &)
        const
{
    if (request.GetMethod() == server::http::HttpMethod::kPost) {
        auto url = ada::parse<ada::url_aggregator>(request.GetArg("url"));
        auto &response = request.GetHttpResponse();
        if (!url || !isValidForWebshotUrl(*url)) {
            LOG_INFO() << fmt::format(
                "Invalid url submitted: {}", url ? url->get_href() : "failed to parse"
            );
            response.SetStatus(server::http::HttpStatus::kBadRequest);
            return {};
        }
        url->set_username("");
        url->set_password("");
        url->clear_hash();
        if (auto hostname = url->get_hostname(); hostname.back() == '.')
            url->set_hostname(std::string_view(begin(hostname), end(hostname) - 1));
        crud.createWebshot(std::string(url->get_href()));
        response.SetStatus(server::http::HttpStatus::kCreated);
        return {};
    }
    const auto uuid = us::utils::BoostUuidFromString(request.GetArg("uuid"));
    auto &response = request.GetHttpResponse();
    auto webshot = crud.findWebshot(uuid);
    if (!webshot) {
        LOG_INFO() << fmt::format("Webshot not found: {}", us::utils::ToString(uuid));
        response.SetStatus(server::http::HttpStatus::kNotFound);
        return {};
    }
    response.SetStatus(server::http::HttpStatus::kFound);
    response.SetHeader(us::http::headers::kLocation, webshot->path);
    return {};
}
