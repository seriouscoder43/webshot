#pragma once

#include "text.hpp"
#include <vector>

namespace v1::s3v4 {

[[nodiscard]] std::vector<std::pair<String, String>> decodeQueryString(String search);

} // namespace v1::s3v4
