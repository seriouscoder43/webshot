#include "integers.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

namespace {

enum class SmallEnum : std::uint8_t {
    kZero = 0,
    kTwo = 2,
};

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <scenario>\n", argv[0]);
        return 64;
    }

    const auto scenario = std::string_view{argv[1]};
    if (scenario == "negative-to-unsigned") {
        static_cast<void>(numericCast<size_t>(-1));
        return 0;
    }
    if (scenario == "narrowing-overflow") {
        static_cast<void>(numericCast<int>(std::numeric_limits<int64_t>::max()));
        return 0;
    }
    if (scenario == "enum-underlying-overflow") {
        static_cast<void>(numericCast<SmallEnum>(std::uint16_t{256}));
        return 0;
    }

    std::fprintf(stderr, "unknown scenario: %s\n", argv[1]);
    return 64;
}
