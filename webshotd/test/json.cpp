#include <string>
#include <vector>

#include <userver/formats/json.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/utest/utest.hpp>

#include "json.hpp"
#include "text.hpp"

namespace v1 {
namespace us = userver;
} // namespace v1

using namespace v1;
using namespace text::literals;

UTEST(Json, ParsesJsonIntoExpected)
{
    const auto json_text = String::FromBytes("7");
    ASSERT_TRUE(json_text);

    const auto parsed = ex::Parse<int>(*json_text, "parse failed"_t);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, 7);
}

UTEST(Json, MapsJsonParseFailure)
{
    const auto json_text = String::FromBytes(R"("bad")");
    ASSERT_TRUE(json_text);

    const auto parsed = exj::Parse<int>(*json_text, "parse failed"_t);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.Error(), "parse failed"_t);
}
