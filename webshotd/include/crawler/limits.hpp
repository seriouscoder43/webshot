#pragma once

#include "integers.hpp"

namespace v1::crawler {

struct [[nodiscard]] CgroupLimits final {
    i64 cpuCores;
    i64 memoryBytes;
};

} // namespace v1::crawler
