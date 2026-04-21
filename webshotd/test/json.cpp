#include <string>
#include <vector>

#include <userver/formats/json.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/utest/utest.hpp>

#include "json.hpp"
#include "text.hpp"
#include "userver_namespaces.hpp"

using namespace text::literals;
namespace exj = v1::exu::json;
using v1::exu::catchException;

UTEST(Json, ParsesJsonIntoExpected)
{
    const auto parsed = exj::parse<int>("7", "parse failed"_t);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, 7);
}

UTEST(Json, MapsJsonParseFailure)
{
    const auto parsed = exj::parse<int>(R"("bad")", "parse failed"_t);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error(), "parse failed"_t);
}

UTEST(Json, ConvertsJsonValueIntoExpected)
{
    const auto value = json::FromString("9");
    const auto parsed = exj::as<int>(value, "shape failed"_t);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, 9);
}

UTEST(Json, StringifiesJsonValueIntoExpected)
{
    const auto text = exj::stringify(std::vector<int>{1, 2, 3}, "serialize failed"_t);
    ASSERT_TRUE(text);
    EXPECT_EQ(*text, std::string{"[1,2,3]"});
}

UTEST(Json, CatchesTypedExceptionWithMappedError)
{
    const auto result = catchException<json::Exception, String>(
        []() -> int { return json::FromString("{").As<int>(); },
        [](const json::Exception &) { return "mapped error"_t; }
    );
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), "mapped error"_t);
}
