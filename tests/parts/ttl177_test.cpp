#include "parts/ttl177.hpp"

#include <catch2/catch_test_macros.hpp>

#include "chip_bench.hpp"
#include "parts/catalogue.hpp"
#include "parts/delays.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// Falling edges every 1 µs from 1 µs: high at k µs + 500 ns, low at (k + 1) µs.
std::vector<Stimulus::Step> Pulses(int count, Time start = 1_us) {
    std::vector<Stimulus::Step> steps;
    for (int k = 0; k < count; ++k) {
        steps.push_back({start + k * 1_us - 500_ns, One});
        steps.push_back({start + k * 1_us, Zero});
    }
    return steps;
}

// Cleared at 0-100 ns, then counting.
const ChipInput kClearFirst{Zero, {{100_ns, One}}};

}  // namespace

TEST_CASE("74177: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74177");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-14");
    Ttl177 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 14);
    CHECK(pins[Ttl177::kLoadN].name == "CT_LOAD_N");
    CHECK(pins[Ttl177::kClk2].name == "CLK2");
    CHECK(pins[6].name == "GND");
    CHECK(pins[Ttl177::kClk1].name == "CLK1");
    CHECK(pins[Ttl177::kClrN].name == "CLR_N");
    CHECK(pins[13].name == "VCC");
    for (const PinSpec& pin : pins) {
        CHECK_FALSE(pin.sampled);
        CHECK_FALSE(pin.risingEdgeOnly);
    }
}

TEST_CASE("74177: CLK1 divides by 2, CLK2 by 8, QA to CLK2 by 16", "[parts]") {
    SECTION("CLK1 and CLK2 apart") {
        ChipBench b;
        b.build<Ttl177>({{"CLR_N", kClearFirst},
                         {"CT_LOAD_N", {One}},
                         {"CLK1", {Zero, Pulses(3)}},
                         {"CLK2", {Zero, Pulses(11)}}},
                        Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        for (int k = 1; k <= 11; ++k) {
            sim.runUntil(k * 1_us + 400_ns);
            INFO("after " << k << " falls");
            CHECK(b.word(sim, {"QB", "QC", "QD"}) == k % 8);
            if (k <= 3) {
                CHECK(b.word(sim, {"QA"}) == k % 2);
            }
        }
    }
    SECTION("QA wired to CLK2") {
        Circuit circuit;
        auto& chip = circuit.add<Ttl177>("U1");
        const auto input = [&](const char* pin, LogicValue initial,
                               std::vector<Stimulus::Step> steps) {
            const NetId net = circuit.addNet(pin);
            auto& s = circuit.add<Stimulus>(std::string("S") + pin, initial, std::move(steps));
            circuit.connect(s, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
        };
        input("CLR_N", Zero, {{100_ns, One}});
        input("CT_LOAD_N", One, {});
        input("CLK1", Zero, Pulses(17));
        for (const char* pin : {"A", "B", "C", "D"}) {
            input(pin, Zero, {});
        }
        const NetId qa = circuit.addNet("QA");
        circuit.connect(chip, "QA", qa);
        circuit.connect(chip, "CLK2", qa);
        std::array<NetId, 3> q{};
        int i = 0;
        for (const char* pin : {"QB", "QC", "QD"}) {
            q[i] = circuit.addNet(pin);
            circuit.connect(chip, pin, q[i++]);
        }
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(1);
        for (int k = 1; k <= 17; ++k) {
            sim.runUntil(k * 1_us + 400_ns);
            int value = sim.value(qa) == One ? 1 : 0;
            for (int bit = 0; bit < 3; ++bit) {
                value |= (sim.value(q[bit]) == One ? 2 : 0) << bit;
            }
            INFO("after " << k << " falls");
            CHECK(value == k % 16);
        }
    }
}

TEST_CASE("74177: the ripple settles QB, then QC, then QD", "[parts]") {
    // 7 → 0 on the eighth CLK2 fall at 8 µs: every stage falls, each later than the last.
    ChipBench b;
    b.build<Ttl177>({{"CLR_N", kClearFirst}, {"CT_LOAD_N", {One}}, {"CLK2", {Zero, Pulses(8)}}},
                    Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(8_us - 1);
    CHECK(b.word(sim, {"QB", "QC", "QD"}) == 7);
    sim.runUntil(8_us + delays::kTtl177Qb.fall - 1);
    CHECK(b.level(sim, "QB") == One);
    sim.runUntil(8_us + delays::kTtl177Qb.fall);
    CHECK(b.level(sim, "QB") == Zero);
    sim.runUntil(8_us + delays::kTtl177Qc.fall - 1);
    CHECK(b.level(sim, "QC") == One);
    sim.runUntil(8_us + delays::kTtl177Qc.fall);
    CHECK(b.level(sim, "QC") == Zero);
    sim.runUntil(8_us + delays::kTtl177Qd.fall - 1);
    CHECK(b.level(sim, "QD") == One);
    sim.runUntil(8_us + delays::kTtl177Qd.fall);
    CHECK(b.level(sim, "QD") == Zero);
}

TEST_CASE("74177: clear over load over count; load transparent", "[parts]") {
    SECTION("load low: Q follows A-D at once, counting stops") {
        // Load 1010 (A=0 B=1 C=0 D=1) from 2.2 to 4.2 µs; D goes to 0 at 3.5 µs. CLK2 falls at
        // 1, 2, ... 6 µs.
        ChipBench b;
        b.build<Ttl177>({{"CLR_N", kClearFirst},
                         {"CT_LOAD_N", {One, {{2200_ns, Zero}, {4200_ns, One}}}},
                         {"B", {One}},
                         {"D", {One, {{3500_ns, Zero}}}},
                         {"CLK2", {Zero, Pulses(6)}}},
                        Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(2200_ns - 1);
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b0100);  // two CLK2 falls
        sim.runUntil(2200_ns + delays::kTtl177Load.fall);  // the slower of the two
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b1010);
        sim.runUntil(3500_ns - 1);  // the CLK2 fall at 3 µs did not count
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b1010);
        sim.runUntil(3500_ns + delays::kTtl177Data.fall - 1);
        CHECK(b.level(sim, "QD") == One);
        sim.runUntil(3500_ns + delays::kTtl177Data.fall);
        CHECK(b.level(sim, "QD") == Zero);
        sim.runUntil(5_us - 1);  // nor the one at 4 µs
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b0010);
        sim.runUntil(5_us + 400_ns);  // counts again from 0010 at 5 µs
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b0100);
    }
    SECTION("clear wins over load") {
        ChipBench b;
        b.build<Ttl177>(
            {{"CLR_N", {One, {{2_us, Zero}}}}, {"CT_LOAD_N", {Zero}}, {"A", {One}}, {"D", {One}}},
            Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(2_us - 1);
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0b1001);
        sim.runUntil(2_us + delays::kTtl177Clear.fall);
        CHECK(b.word(sim, {"QA", "QB", "QC", "QD"}) == 0);
    }
}

TEST_CASE("74177: X clock and random power-on", "[parts]") {
    SECTION("X on CLK2: QB-QD unknown, QA kept") {
        ChipBench b;
        b.build<Ttl177>(
            {{"CLR_N", kClearFirst}, {"CT_LOAD_N", {One}}, {"CLK2", {Zero, {{1_us, X}}}}}, Zero);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(2_us);
        CHECK(b.level(sim, "QA") == Zero);
        CHECK(b.level(sim, "QB") == X);
        CHECK(b.level(sim, "QD") == X);
    }
    SECTION("power-on draws the count") {
        bool differ = false;
        int first = -1;
        for (std::uint64_t seed = 1; seed <= 8; ++seed) {
            ChipBench b;
            b.build<Ttl177>({{"CLR_N", {One}}, {"CT_LOAD_N", {One}}}, Zero);
            Simulator sim(b.circuit);
            sim.powerOn(seed);
            sim.runUntil(100_ns);
            const int value = b.word(sim, {"QA", "QB", "QC", "QD"});
            REQUIRE(value >= 0);
            differ |= first >= 0 && value != first;
            first = value;
        }
        CHECK(differ);
    }
}
