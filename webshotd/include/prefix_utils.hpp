#pragma once

#include "link.hpp"
#include "text.hpp"

#include <string>

namespace v1::prefix {

[[nodiscard]] String MakePrefixKey(const Link &link);
[[nodiscard]] std::string MakePrefixTree(const String &prefix_key);

} // namespace v1::prefix
