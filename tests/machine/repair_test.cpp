#include "machine/repair.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("repair: parse swap and mend", "[machine]") {
    const auto swap = ParseRepair("swap:U3");
    REQUIRE(swap.has_value());
    CHECK(std::get<SwapRepair>(*swap).reference == "U3");

    const auto front = ParseRepair("mend:F.Cu:52.30:41.00");
    REQUIRE(front.has_value());
    const auto& mend = std::get<MendRepair>(*front);
    CHECK(mend.layer == Layer::Front);
    CHECK(mend.at == Point{52'300'000, 41'000'000});

    const auto back = ParseRepair("mend:B.Cu:1:2");
    REQUIRE(back.has_value());
    CHECK(std::get<MendRepair>(*back).layer == Layer::Back);
    CHECK(std::get<MendRepair>(*back).at == Point{1'000'000, 2'000'000});
}

TEST_CASE("repair: parse errors", "[machine]") {
    for (const char* text : {"cut:U1", "swap:", "mend:X.Cu:1:2", "mend:F.Cu:1", "mend:F.Cu:a:2"}) {
        INFO(text);
        const auto repair = ParseRepair(text);
        REQUIRE_FALSE(repair.has_value());
        CHECK(repair.error() == "bad repair '" + std::string(text) + "'");
    }
}

TEST_CASE("repair: format", "[machine]") {
    CHECK(FormatRepair(SwapRepair{"U3"}) == "swap U3");
    CHECK(FormatRepair(MendRepair{Layer::Front, {52'300'000, 41'000'000}}) ==
          "mend F.Cu 52.30 41.00");
}
