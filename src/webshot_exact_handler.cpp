#include "include/webshot_exact_handler.hpp"
#include "include/http_utils.hpp"
#include "include/link.hpp"
#include "include/server_errors.hpp"

#include <string>

#include <userver/components/component.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

namespace us = userver;

using namespace v1;

WebshotExactHandler::WebshotExactHandler(
    const us::components::ComponentConfig &config, const us::components::ComponentContext &context
)
    : HttpHandlerBase(config, context), crud(context.FindComponent<WebshotCrud>()),
      cfg(context.FindComponent<WebshotConfig>())
{
}

std::string WebshotExactHandler::
    HandleRequestThrow(const server::http::HttpRequest &request, server::request::RequestContext &)
        const
{
    using server::http::HttpMethod::kGet;
    using server::http::HttpStatus::kBadRequest;
    using server::http::HttpStatus::kInternalServerError;
    using server::http::HttpStatus::kMethodNotAllowed;
    using server::http::HttpStatus::kOk;
    using us::http::content_type::kApplicationJson;

    auto &response = request.GetHttpResponse();
    try {
        const std::string linkArg = request.GetArg("link");
        if (linkArg.empty())
            return httpu::respondError(response, kBadRequest, "missing parameter: link");
        Link link;
        try {
            link = Link::fromUserInput(linkArg, cfg.queryPartLengthMax());
        } catch (const InvalidLinkException &e) {
            return httpu::respondError(response, kBadRequest, e.what());
        }
        const auto token = request.GetArg("page_token");
        try {
            auto page = crud.findWebshotByLinkPage(
                link, token.empty() ? std::nullopt : std::make_optional(token)
            );
            return httpu::respondJson(response, kOk, page);
        } catch (const errors::InvalidPageTokenException &) {
            return httpu::respondError(response, kBadRequest, "invalid page_token");
        }
    } catch (const std::exception &e) {
        LOG_ERROR() << "Unhandled error in webshot_exact_handler: " << e.what();
        return httpu::respondError(response, kInternalServerError, "internal server error");
    }
}
