#include "parts/ttl100.hpp"

#include <catch2/catch_test_macros.hpp>

#include "chip_bench.hpp"
#include "parts/catalogue.hpp"
#include "parts/delays.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

int Half1(const ChipBench& b, const Simulator& sim) {
    return b.word(sim, {"1Q1", "1Q2", "1Q3", "1Q4"});
}
int Half2(const ChipBench& b, const Simulator& sim) {
    return b.word(sim, {"2Q1", "2Q2", "2Q3", "2Q4"});
}

}  // namespace

TEST_CASE("74100: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74100");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-24");
    Ttl100 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 24);
    CHECK(pins[0].kind == PinKind::NoConnect);
    CHECK(pins[6].name == "GND");
    CHECK(pins[Ttl100::kEnable[0]].name == "1G");
    CHECK(pins[Ttl100::kEnable[1]].name == "2G");
    CHECK(pins[Ttl100::kD[0][0]].name == "1D1");
    CHECK(pins[Ttl100::kD[0][3]].name == "1D4");
    CHECK(pins[Ttl100::kD[1][2]].name == "2D3");
    CHECK(pins[Ttl100::kQ[0][0]].name == "1Q1");
    CHECK(pins[Ttl100::kQ[1][3]].name == "2Q4");
    CHECK(pins[23].name == "VCC");
}

TEST_CASE("74100: transparent while G is high, holds the D from before its fall", "[parts]") {
    // 1G high until 2 µs; 1D1 rises at 1 µs, 1D3 at 2 µs (same time as the fall: not taken),
    // 1D2 at 3 µs (held: not taken). 2G stays low: the second half holds its power-on bits.
    ChipBench b;
    b.build<Ttl100>({{"1G", {One, {{2_us, Zero}}}},
                     {"1D1", {Zero, {{1_us, One}}}},
                     {"1D3", {Zero, {{2_us, One}}}},
                     {"1D2", {Zero, {{3_us, One}}}},
                     {"2D1", {One}},
                     {"2D2", {One}},
                     {"2D3", {One}},
                     {"2D4", {One}}},
                    Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(500_ns);
    CHECK(Half1(b, sim) == 0);
    const int second = Half2(b, sim);
    REQUIRE(second >= 0);
    sim.runUntil(1_us + delays::kTtl100Data.rise - 1);
    CHECK(Half1(b, sim) == 0);
    sim.runUntil(1_us + delays::kTtl100Data.rise);
    CHECK(Half1(b, sim) == 0b0001);
    sim.runUntil(4_us);
    CHECK(Half1(b, sim) == 0b0001);
    CHECK(Half2(b, sim) == second);
}

TEST_CASE("74100: enable delay and the halves apart", "[parts]") {
    // 2G rises at 1 µs with 2D = 1111: Q after the enable delay; the first half stays held.
    ChipBench b;
    b.build<Ttl100>({{"2G", {Zero, {{1_us, One}}}},
                     {"1G", {One, {{500_ns, Zero}}}},
                     {"2D1", {One}},
                     {"2D2", {One}},
                     {"2D3", {One}},
                     {"2D4", {One}}},
                    Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + delays::kTtl100Enable.rise - 1);
    CHECK(Half1(b, sim) == 0);
    sim.runUntil(1_us + delays::kTtl100Enable.rise);
    CHECK(Half2(b, sim) == 0b1111);
    CHECK(Half1(b, sim) == 0);
}

TEST_CASE("74100: X enable makes Q X where D and the held bit differ", "[parts]") {
    // Held 0011 (1D1, 1D2 high), then 1G at X with D = 0101.
    ChipBench b;
    b.build<Ttl100>({{"1G", {One, {{1_us, Zero}, {2_us, X}}}},
                     {"1D1", {One}},
                     {"1D2", {One, {{1500_ns, Zero}}}},
                     {"1D3", {Zero, {{1500_ns, One}}}}},
                    Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(3_us);
    CHECK(b.level(sim, "1Q1") == One);
    CHECK(b.level(sim, "1Q2") == X);
    CHECK(b.level(sim, "1Q3") == X);
    CHECK(b.level(sim, "1Q4") == Zero);
}
