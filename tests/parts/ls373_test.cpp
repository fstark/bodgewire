#include "parts/ls373.hpp"

#include <array>
#include <string>
#include <string_view>

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

TEST_CASE("74LS373: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS373");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-20");
    Ls373 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 20);
    const std::array<std::string_view, 20> names{"OE_N", "1Q", "1D",  "2D", "2Q", "3Q", "3D",
                                                 "4D",   "4Q", "GND", "LE", "5Q", "5D", "6D",
                                                 "6Q",   "7Q", "7D",  "8D", "8Q", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        CHECK(pins[i].name == names[i]);
        CHECK_FALSE(pins[i].sampled);  // the Ds act while LE is high
    }
}

TEST_CASE("74LS373: transparent while LE is high, then holds", "[parts]") {
    // LE high until 2 us; 1D high at 1 us, low at 3 us (ignored); 8D high at 3 us (ignored).
    ChipBench b;
    b.build<Ls373>({{"LE", {One, {{2_us, Zero}}}},
                    {"1D", {Zero, {{1_us, One}, {3_us, Zero}}}},
                    {"8D", {Zero, {{3_us, One}}}}},
                   Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(900_ns);
    CHECK(Shown(b, sim, "1Q") == Zero);
    sim.runUntil(1_us + kLs373Data.rise);
    CHECK(Shown(b, sim, "1Q") == One);
    sim.runUntil(4_us);
    CHECK(Shown(b, sim, "1Q") == One);   // held
    CHECK(Shown(b, sim, "8Q") == Zero);  // held
}

TEST_CASE("74LS373: holds the D from just before LE falls", "[parts]") {
    // 1D changes at the same time LE falls: the latch keeps the old 1D.
    ChipBench b;
    b.build<Ls373>({{"LE", {One, {{2_us, Zero}}}}, {"1D", {One, {{2_us, Zero}}}}}, Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(3_us);
    CHECK(Shown(b, sim, "1Q") == One);
}

TEST_CASE("74LS373: OE_N turns the outputs off, not the latch", "[parts]") {
    // Latched 1 at 1 us; off from 2 us; 1D changes at 3 us (latched, ignored); on again at 4 us.
    ChipBench b;
    b.build<Ls373>({{"OE_N", {Zero, {{2_us, One}, {4_us, Zero}}}},
                    {"LE", {One, {{1_us, Zero}}}},
                    {"1D", {One, {{3_us, Zero}}}}},
                   Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(2_us + kLs373Disable.rise - 1);
    CHECK(Shown(b, sim, "1Q") == One);
    sim.runUntil(2_us + kLs373Disable.rise);
    CHECK(Shown(b, sim, "1Q") == Z);
    sim.runUntil(4_us + kLs373Enable.rise - 1);
    CHECK(Shown(b, sim, "1Q") == Z);
    sim.runUntil(4_us + kLs373Enable.rise);
    CHECK(Shown(b, sim, "1Q") == One);
}

TEST_CASE("74LS373: LE rising shows D after the latch delay", "[parts]") {
    // Latched 0 (LE low from 0); 1D high at 1 us; LE rises at 2 us.
    ChipBench b;
    b.build<Ls373>({{"LE", {One, {{500_ns, Zero}, {2_us, One}}}}, {"1D", {Zero, {{1_us, One}}}}},
                   Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(2_us + kLs373Latch.rise - 1);
    CHECK(Shown(b, sim, "1Q") == Zero);
    sim.runUntil(2_us + kLs373Latch.rise);
    CHECK(Shown(b, sim, "1Q") == One);
}

TEST_CASE("74LS373: X cases", "[parts]") {
    SECTION("LE at X: a bit whose D differs from it goes X") {
        ChipBench b;
        b.build<Ls373>({{"LE", {One, {{1_us, X}}}}, {"1D", {Zero, {{2_us, One}}}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1500_ns);
        CHECK(Shown(b, sim, "1Q") == Zero);  // D agrees
        sim.runUntil(3_us);
        CHECK(Shown(b, sim, "1Q") == X);
    }
    SECTION("OE_N at X: X out") {
        ChipBench b;
        b.build<Ls373>({{"OE_N", {X}}, {"LE", {One}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(Shown(b, sim, "1Q") == X);
    }
}
