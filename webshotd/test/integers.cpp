#include "integers.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

#include <userver/utest/utest.hpp>

namespace {

enum class SmallEnum : std::uint8_t {
    kZero = 0,
    kTwo = 2,
};

} // namespace

UTEST(Integers, NumericCastSupportsIntegralConversions)
{
    EXPECT_EQ(numericCast<int64_t>(123), int64_t{123});
    EXPECT_EQ(numericCast<size_t>(int64_t{3}), size_t{3});
    EXPECT_EQ(numericCast<int>(size_t{7}), 7);
}

UTEST(Integers, NumericCastSupportsEnumConversions)
{
    EXPECT_EQ(numericCast<unsigned int>(SmallEnum::kTwo), 2U);
    EXPECT_EQ(numericCast<SmallEnum>(std::uint16_t{2}), SmallEnum::kTwo);
}

UTEST(Integers, NumericCastSupportsSafeIntegerSources)
{
    const auto value = i64{123};
    const auto enumValue = u16{2};

    EXPECT_EQ(numericCast<int64_t>(value), int64_t{123});
    EXPECT_EQ(numericCast<SmallEnum>(enumValue), SmallEnum::kTwo);
}

UTEST(Integers, NumericCastAbortsOnNegativeToUnsigned)
{
    EXPECT_DEATH(static_cast<void>(numericCast<size_t>(-1)), "safe integer failure");
}

UTEST(Integers, NumericCastAbortsOnNarrowingOverflow)
{
    EXPECT_DEATH(
        static_cast<void>(numericCast<int>(std::numeric_limits<int64_t>::max())),
        "safe integer failure"
    );
}

UTEST(Integers, NumericCastAbortsOnEnumUnderlyingOverflow)
{
    EXPECT_DEATH(
        static_cast<void>(numericCast<SmallEnum>(std::uint16_t{256})), "safe integer failure"
    );
}
