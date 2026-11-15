#include "parts/ls365.hpp"

#include <array>
#include <string>
#include <string_view>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

#include "chip_bench.hpp"
#include "parts/catalogue.hpp"
#include "parts/delays.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using namespace delays;

LogicValue Shown(const ChipBench& b, const Simulator& sim, std::string_view pin) {
    const NetId net = b.nets.find(pin)->second;
    return sim.isFloating(net) ? Z : sim.value(net);
}

}  // namespace

TEST_CASE("74LS365: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS365");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    Ls365 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    const std::array<std::string_view, 16> names{"G1_N", "A1",  "Y1",   "A2", "Y2", "A3",
                                                 "Y3",   "GND", "Y4",   "A4", "Y5", "A5",
                                                 "Y6",   "A6",  "G2_N", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        CHECK(pins[i].name == names[i]);
    }
}

TEST_CASE("74LS365: on only while both enables are low", "[parts]") {
    for (const auto& [g1, g2, on] : std::array<std::tuple<LogicValue, LogicValue, bool>, 4>{
             {{Zero, Zero, true}, {Zero, One, false}, {One, Zero, false}, {One, One, false}}}) {
        ChipBench b;
        b.build<Ls365>({{"G1_N", {g1}}, {"G2_N", {g2}}, {"A1", {One}}, {"A6", {One}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        if (on) {
            CHECK(Shown(b, sim, "Y1") == One);
            CHECK(Shown(b, sim, "Y2") == Zero);
            CHECK(Shown(b, sim, "Y6") == One);
        } else {
            CHECK(Shown(b, sim, "Y1") == Z);
            CHECK(Shown(b, sim, "Y6") == Z);
        }
    }
}

TEST_CASE("74LS365: X cases", "[parts]") {
    SECTION("an X enable with the other low: X out") {
        ChipBench b;
        b.build<Ls365>({{"G1_N", {X}}, {"A1", {One}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(Shown(b, sim, "Y1") == X);
    }
    SECTION("an X enable with the other high: off") {
        ChipBench b;
        b.build<Ls365>({{"G1_N", {X}}, {"G2_N", {One}}, {"A1", {One}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(Shown(b, sim, "Y1") == Z);
    }
    SECTION("an X input: X out") {
        ChipBench b;
        b.build<Ls365>({{"A3", {X}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(Shown(b, sim, "Y3") == X);
    }
}

TEST_CASE("74LS365: data, enable and disable delays", "[parts]") {
    // On at 1 us, A1 high at 2 us, low at 3 us, off at 4 us.
    ChipBench b;
    b.build<Ls365>(
        {{"G1_N", {One, {{1_us, Zero}, {4_us, One}}}}, {"A1", {Zero, {{2_us, One}, {3_us, Zero}}}}},
        Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + kLs365Enable.fall - 1);
    CHECK(Shown(b, sim, "Y1") == Z);
    sim.runUntil(1_us + kLs365Enable.fall);
    CHECK(Shown(b, sim, "Y1") == Zero);
    sim.runUntil(2_us + kLs365Data.rise - 1);
    CHECK(Shown(b, sim, "Y1") == Zero);
    sim.runUntil(2_us + kLs365Data.rise);
    CHECK(Shown(b, sim, "Y1") == One);
    sim.runUntil(3_us + kLs365Data.fall - 1);
    CHECK(Shown(b, sim, "Y1") == One);
    sim.runUntil(3_us + kLs365Data.fall);
    CHECK(Shown(b, sim, "Y1") == Zero);
    sim.runUntil(4_us + kLs365Disable.fall - 1);
    CHECK(Shown(b, sim, "Y1") == Zero);
    sim.runUntil(4_us + kLs365Disable.fall);
    CHECK(Shown(b, sim, "Y1") == Z);
}
