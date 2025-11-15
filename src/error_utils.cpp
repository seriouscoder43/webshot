#include "include/error_utils.hpp"
#include "schemas/webshot.hpp"

#include <userver/formats/json/value_builder.hpp>

namespace json = userver::formats::json;

namespace v1::errors {

json::Value makeError(std::string_view message)
{
    dto::ErrorEnvelope::Error err{std::string(message)};
    dto::ErrorEnvelope env{err};
    return json::ValueBuilder(env).ExtractValue();
}

json::Value makeParamError(std::string_view fieldName, std::string_view message)
{
    return makeError(std::string(message) + ": " + std::string(fieldName));
}

} // namespace v1::errors
