#pragma once

#include "link.hpp"
#include "text.hpp"

#include <string>

namespace v1::prefix {

[[nodiscard]] String makePrefixKey(const Link &link);
[[nodiscard]] std::string makePrefixTree(const String &prefixKey);

} // namespace v1::prefix
