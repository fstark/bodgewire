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

TEST_CASE("repair: cut, wire and unwire round trips", "[machine]") {
    const auto cut = ParseRepair("cut:F.Cu:52.30:41.00");
    REQUIRE(cut.has_value());
    CHECK(std::get<CutRepair>(*cut).at == Point{52'300'000, 41'000'000});
    CHECK(FormatRepair(*cut) == "cut F.Cu 52.30 41.00");

    const auto back = ParseRepair("cut:B.Cu:1:2");
    REQUIRE(back.has_value());
    CHECK(std::get<CutRepair>(*back).layer == Layer::Back);
    CHECK(FormatRepair(*back) == "cut B.Cu 1.00 2.00");

    const auto wire = ParseRepair("wire:U1.14:U13.6");
    REQUIRE(wire.has_value());
    CHECK(std::get<WireRepair>(*wire).a.reference == "U1");
    CHECK(std::get<WireRepair>(*wire).b.pin == "6");
    CHECK(FormatRepair(*wire) == "wire U1.14 U13.6");

    const auto unwire = ParseRepair("unwire:U1.QA:U13.6");
    REQUIRE(unwire.has_value());
    CHECK(std::get<UnwireRepair>(*unwire).a.pin == "QA");
    CHECK(FormatRepair(*unwire) == "unwire U1.QA U13.6");
}

TEST_CASE("repair: cut and wire parse errors", "[machine]") {
    for (const char* text : {"cut:F.Cu:1", "cut:X.Cu:1:2", "wire:U1.14", "wire:U1:U2.3",
                             "wire:.14:U2.3", "wire:U1.14:U1.14", "unwire:U1.:U2.3"}) {
        INFO(text);
        const auto repair = ParseRepair(text);
        REQUIRE_FALSE(repair.has_value());
        CHECK(repair.error() == "bad repair '" + std::string(text) + "'");
    }
}
