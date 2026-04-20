#include <memory>
#include <optional>

#include <userver/utest/utest.hpp>

#include "try.hpp"

namespace {

enum class TestError {
    kAlpha,
    kBeta,
};

struct MoveOnly final {
    explicit MoveOnly(int v) : value(std::make_unique<int>(v)) {}

    MoveOnly(const MoveOnly &) = delete;
    MoveOnly(MoveOnly &&) noexcept = default;
    MoveOnly &operator=(const MoveOnly &) = delete;
    MoveOnly &operator=(MoveOnly &&) noexcept = default;

    std::unique_ptr<int> value;
};

[[nodiscard]] v1::Expected<int, TestError> makeValue(bool ok)
{
    if (!ok)
        return v1::Unex(TestError::kAlpha);
    return 21;
}

[[nodiscard]] v1::Expected<void, TestError> makeVoid(bool ok)
{
    if (!ok)
        return v1::Unex(TestError::kBeta);
    return {};
}

[[nodiscard]] std::optional<int> maybeValue(bool ok)
{
    if (!ok)
        return {};
    return 7;
}

[[nodiscard]] std::optional<std::unique_ptr<int>> maybePointer(bool ok)
{
    if (!ok)
        return {};
    return std::make_unique<int>(13);
}

[[nodiscard]] v1::Expected<MoveOnly, TestError> makeMoveOnly(bool ok)
{
    if (!ok)
        return v1::Unex(TestError::kBeta);
    return MoveOnly{33};
}

[[nodiscard]] v1::Expected<int, TestError> doubleExpected(bool ok)
{
    return TRY(makeValue(ok)) * 2;
}

[[nodiscard]] v1::Expected<int, TestError> propagateVoid(bool ok)
{
    TRY(makeVoid(ok));
    return 9;
}

[[nodiscard]] std::optional<int> doubleOptional(bool ok) { return TRY(maybeValue(ok)) * 2; }

[[nodiscard]] std::optional<int> optionalFromExpected(bool ok) { return TRY(makeValue(ok)) * 2; }

[[nodiscard]] std::optional<int> optionalFromExpectedVoid(bool ok)
{
    TRY(makeVoid(ok));
    return 9;
}

[[nodiscard]] std::optional<int> lvalueOptional(bool ok)
{
    auto value = maybeValue(ok);
    return TRY(value) + 1;
}

[[nodiscard]] std::optional<int> lvalueOptionalFromExpected(bool ok)
{
    auto value = makeValue(ok);
    return TRY(value) + 1;
}

[[nodiscard]] v1::Expected<int, TestError> lvalueExpected(bool ok)
{
    auto value = makeValue(ok);
    return TRY(value) + 1;
}

[[nodiscard]] v1::Expected<int, TestError> twoTrys(bool leftOk, bool rightOk)
{
    return TRY(makeValue(leftOk)) + TRY(makeValue(rightOk));
}

[[nodiscard]] v1::Expected<int, TestError> singleEvaluation(int &calls, bool ok)
{
    return TRY(([&]() -> v1::Expected<int, TestError> {
        calls++;
        if (!ok)
            return v1::Unex(TestError::kBeta);
        return 5;
    })());
}

[[nodiscard]] v1::Expected<int, TestError> unwrapMoveOnly(bool ok)
{
    auto value = TRY(makeMoveOnly(ok));
    return *value.value;
}

[[nodiscard]] std::optional<int> unwrapOptionalMoveOnly(bool ok)
{
    auto value = TRY(maybePointer(ok));
    return *value;
}

} // namespace

UTEST(Try, PropagatesExpectedValueErrors)
{
    const auto value = doubleExpected(false);
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error(), TestError::kAlpha);
}

UTEST(Try, UnwrapsExpectedValueInExpressionPosition)
{
    const auto value = doubleExpected(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
}

UTEST(Try, PropagatesExpectedVoidErrors)
{
    const auto value = propagateVoid(false);
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error(), TestError::kBeta);
}

UTEST(Try, SupportsExpectedVoidStatements)
{
    const auto value = propagateVoid(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 9);
}

UTEST(Try, PropagatesEmptyOptional)
{
    const auto value = doubleOptional(false);
    ASSERT_FALSE(value);
}

UTEST(Try, UnwrapsOptionalInExpressionPosition)
{
    const auto value = doubleOptional(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 14);
}

UTEST(Try, ConvertsExpectedFailureToEmptyOptional)
{
    const auto value = optionalFromExpected(false);
    ASSERT_FALSE(value);
}

UTEST(Try, UnwrapsExpectedIntoOptional)
{
    const auto value = optionalFromExpected(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
}

UTEST(Try, ConvertsExpectedVoidFailureToEmptyOptional)
{
    const auto value = optionalFromExpectedVoid(false);
    ASSERT_FALSE(value);
}

UTEST(Try, UnwrapsExpectedVoidIntoOptional)
{
    const auto value = optionalFromExpectedVoid(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 9);
}

UTEST(Try, SupportsLvalueOptional)
{
    const auto value = lvalueOptional(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 8);
}

UTEST(Try, SupportsExpectedLvalueInOptionalReturn)
{
    const auto value = lvalueOptionalFromExpected(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 22);
}

UTEST(Try, ConvertsExpectedLvalueFailureToEmptyOptional)
{
    const auto value = lvalueOptionalFromExpected(false);
    ASSERT_FALSE(value);
}

UTEST(Try, PropagatesLvalueExpectedErrors)
{
    const auto value = lvalueExpected(false);
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error(), TestError::kAlpha);
}

UTEST(Try, SupportsMultipleExpressionUses)
{
    const auto value = twoTrys(true, true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
}

UTEST(Try, StopsAtFirstFailure)
{
    const auto value = twoTrys(false, true);
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error(), TestError::kAlpha);
}

UTEST(Try, EvaluatesExpressionOnceOnSuccess)
{
    int calls = 0;
    const auto value = singleEvaluation(calls, true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 5);
    EXPECT_EQ(calls, 1);
}

UTEST(Try, EvaluatesExpressionOnceOnFailure)
{
    int calls = 0;
    const auto value = singleEvaluation(calls, false);
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error(), TestError::kBeta);
    EXPECT_EQ(calls, 1);
}

UTEST(Try, UnwrapsMoveOnlyExpectedValues)
{
    const auto value = unwrapMoveOnly(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 33);
}

UTEST(Try, UnwrapsMoveOnlyOptionalValues)
{
    const auto value = unwrapOptionalMoveOnly(true);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 13);
}

UTEST(Try, PropagatesEmptyOptionalForMoveOnlyValues)
{
    const auto value = unwrapOptionalMoveOnly(false);
    ASSERT_FALSE(value);
}
