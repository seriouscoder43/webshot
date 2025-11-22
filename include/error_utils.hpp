#pragma once

#include <string_view>

#include <userver/formats/json/value.hpp>

namespace v1::errors {

/**
 * @brief Build a standard JSON error envelope with a message.
 *
 * The shape matches the OpenAPI schema under `ErrorEnvelope`.
 */
userver::formats::json::Value makeError(std::string_view message);

/**
 * @brief Convenience for parameter‑specific errors.
 *
 * @param fieldName Name of the offending parameter.
 * @param message Additional context to append to the message.
 */
userver::formats::json::Value makeParamError(std::string_view fieldName, std::string_view message);

} // namespace v1::errors
