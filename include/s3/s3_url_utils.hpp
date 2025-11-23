#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace v1::s3v4 {

[[nodiscard]] std::vector<std::pair<std::string, std::string>>
decodeQueryString(std::string_view search);

} // namespace v1::s3v4
