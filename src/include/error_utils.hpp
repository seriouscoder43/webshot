#pragma once

#include <string_view>

#include <userver/formats/json/value.hpp>

namespace v1::errors {

userver::formats::json::Value makeError(std::string_view message);

userver::formats::json::Value makeParamError(std::string_view param_name, std::string_view detail);

} // namespace v1::errors
