#pragma once

#include "integers.hpp"

namespace v1::crawler {

struct [[nodiscard]] CgroupLimits final {
    i64 cpu_cores;
    i64 memory_bytes;
};

} // namespace v1::crawler
