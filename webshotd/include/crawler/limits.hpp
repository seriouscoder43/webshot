#pragma once

#include "integers.hpp"

#include <string>
#include <string_view>

namespace v1::crawler {

struct [[nodiscard]] CgroupLimits final {
    i64 cpuCores;
    i64 memoryBytes;
};

inline constexpr std::string_view kBwrapStatusWrapperScript = R"WSBWRAP(
	set -euo pipefail
	status_path="$1"
	shift 1
	exec 3>"$status_path"
	exec "$@"
	)WSBWRAP";

} // namespace v1::crawler
