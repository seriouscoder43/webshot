#include "integers.hpp"
#include "subprocess_probe.hpp"

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

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
    const auto result = test::subprocess_probe::run(
        "integers_abort_probe", {"negative-to-unsigned"}
    );
    EXPECT_TRUE(result.signaled());
    EXPECT_EQ(result.termSignal, SIGABRT);
    EXPECT_NE(result.output.find("safe integer failure"), std::string::npos);
}

UTEST(Integers, NumericCastAbortsOnNarrowingOverflow)
{
    const auto result = test::subprocess_probe::run("integers_abort_probe", {"narrowing-overflow"});
    EXPECT_TRUE(result.signaled());
    EXPECT_EQ(result.termSignal, SIGABRT);
    EXPECT_NE(result.output.find("safe integer failure"), std::string::npos);
}

UTEST(Integers, NumericCastAbortsOnEnumUnderlyingOverflow)
{
    const auto result = test::subprocess_probe::run(
        "integers_abort_probe", {"enum-underlying-overflow"}
    );
    EXPECT_TRUE(result.signaled());
    EXPECT_EQ(result.termSignal, SIGABRT);
    EXPECT_NE(result.output.find("safe integer failure"), std::string::npos);
}
