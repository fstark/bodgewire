#include "parts/ls154.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "chip_bench.hpp"
#include "parts/catalogue.hpp"
#include "parts/delays.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

LogicValue Bit(int value, int bit) { return (value >> bit & 1) != 0 ? One : Zero; }

std::string Y(int k) { return "Y" + std::to_string(k) + "_N"; }

}  // namespace

TEST_CASE("74LS154: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS154");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-24");
    Ls154 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 24);
    CHECK(pins[0].name == "Y0_N");
    CHECK(pins[10].name == "Y10_N");
    CHECK(pins[11].name == "GND");
    CHECK(pins[12].name == "Y11_N");
    CHECK(pins[16].name == "Y15_N");
    CHECK(pins[Ls154::kG1N].name == "G1_N");
    CHECK(pins[Ls154::kG2N].name == "G2_N");
    CHECK(pins[Ls154::kD].name == "D");
    CHECK(pins[Ls154::kA].name == "A");
    CHECK(pins[23].name == "VCC");
}

TEST_CASE("74LS154: each select value pulls its own output low", "[parts]") {
    const int value = GENERATE(range(0, 16));
    INFO("select " << value);
    ChipBench b;
    b.build<Ls154>({{"A", {Bit(value, 0)}},
                    {"B", {Bit(value, 1)}},
                    {"C", {Bit(value, 2)}},
                    {"D", {Bit(value, 3)}}},
                   Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    for (int k = 0; k < 16; ++k) {
        CHECK(b.level(sim, Y(k)) == (k == value ? Zero : One));
    }
}

TEST_CASE("74LS154: either enable high puts every output high", "[parts]") {
    const std::string pin = GENERATE("G1_N", "G2_N");
    INFO(pin);
    // Select 9; the enable goes high at 100 ns and low again at 200 ns.
    ChipBench b;
    b.build<Ls154>({{"A", {One}}, {"D", {One}}, {pin, {Zero, {{100_ns, One}, {200_ns, Zero}}}}},
                   Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(b.level(sim, "Y9_N") == Zero);
    sim.runUntil(100_ns + delays::kLs154Enable.rise - 1);
    CHECK(b.level(sim, "Y9_N") == Zero);
    sim.runUntil(100_ns + delays::kLs154Enable.rise);
    for (int k = 0; k < 16; ++k) {
        CHECK(b.level(sim, Y(k)) == One);
    }
    sim.runUntil(200_ns + delays::kLs154Enable.fall - 1);
    CHECK(b.level(sim, "Y9_N") == One);
    sim.runUntil(200_ns + delays::kLs154Enable.fall);
    CHECK(b.level(sim, "Y9_N") == Zero);
}

TEST_CASE("74LS154: select delay", "[parts]") {
    // 0 → 12 at 100 ns.
    ChipBench b;
    b.build<Ls154>({{"C", {Zero, {{100_ns, One}}}}, {"D", {Zero, {{100_ns, One}}}}}, Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + delays::kLs154Select.fall - 1);
    CHECK(b.level(sim, "Y12_N") == One);
    CHECK(b.level(sim, "Y0_N") == Zero);
    sim.runUntil(100_ns + delays::kLs154Select.fall);
    CHECK(b.level(sim, "Y12_N") == Zero);
    sim.runUntil(100_ns + delays::kLs154Select.rise);
    CHECK(b.level(sim, "Y0_N") == One);
}

TEST_CASE("74LS154: X only where it could matter", "[parts]") {
    SECTION("X on D: the two outputs it could choose are X") {
        ChipBench b;
        b.build<Ls154>({{"A", {One}}, {"D", {X}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 16; ++k) {
            CHECK(b.level(sim, Y(k)) == (k == 1 || k == 9 ? X : One));
        }
    }
    SECTION("X on an enable: the selected output X") {
        ChipBench b;
        b.build<Ls154>({{"B", {One}}, {"G2_N", {X}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 16; ++k) {
            CHECK(b.level(sim, Y(k)) == (k == 2 ? X : One));
        }
    }
    SECTION("X on an enable while the other is high: all high") {
        ChipBench b;
        b.build<Ls154>({{"G1_N", {One}}, {"G2_N", {X}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 16; ++k) {
            CHECK(b.level(sim, Y(k)) == One);
        }
    }
}
