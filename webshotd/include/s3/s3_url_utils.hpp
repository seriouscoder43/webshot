#pragma once

#include "expected.hpp"
#include "text.hpp"

#include <vector>

namespace v1::s3v4 {

using v1::Expected;

enum class QueryStringError {
    kInvalidUtf8Key,
    kInvalidUtf8Value,
};

[[nodiscard]] Expected<std::vector<std::pair<String, String>>, QueryStringError>
decodeQueryString(String search);

} // namespace v1::s3v4
