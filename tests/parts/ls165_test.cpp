#include "parts/ls165.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

constexpr const char* kInputs[] = {"SH_LD_N", "CLK", "CLK_INH", "SER", "A", "B",
                                   "C",       "D",   "E",       "F",   "G", "H"};

// One 74LS165; every input driven (listed, else 0); QH and QH_N on Nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls165>("U1");
        for (const char* pin : kInputs) {
            Input input{Zero, {}};
            if (const auto it = inputs.find(pin); it != inputs.end()) {
                input = it->second;
            }
            const NetId net = circuit.addNet(pin);
            auto& source =
                circuit.add<Stimulus>(std::string("S") + pin, input.initial, input.steps);
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : {"QH", "QH_N"}) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

// Rising CLK edges at 1, 2, … n µs (high for 500 ns).
std::vector<Stimulus::Step> Clocks(int n) {
    std::vector<Stimulus::Step> steps;
    for (int i = 1; i <= n; ++i) {
        steps.push_back({i * 1_us, One});
        steps.push_back({i * 1_us + 500_ns, Zero});
    }
    return steps;
}

}  // namespace

TEST_CASE("74LS165: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS165");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ls165 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[Ls165::kShLdN].name == "SH_LD_N");
    CHECK(pins[Ls165::kClk].name == "CLK");
    CHECK(pins[Ls165::kClk].risingEdgeOnly);
    CHECK(pins[Ls165::kH].name == "H");
    CHECK(pins[Ls165::kQHN].name == "QH_N");
    CHECK(pins[7].name == "GND");
    CHECK(pins[Ls165::kQH].name == "QH");
    CHECK(pins[Ls165::kQH].kind == PinKind::Output);
    CHECK(pins[Ls165::kSer].name == "SER");
    CHECK(pins[Ls165::kA].name == "A");
    CHECK(pins[Ls165::kD].name == "D");
    CHECK(pins[Ls165::kClkInh].name == "CLK_INH");
    CHECK(pins[15].name == "VCC");
}

TEST_CASE("74LS165: load, then shift H to A out, then SER", "[parts]") {
    // Load 1,0,1,1,0,0,1,0 into A..H while SH_LD_N is low (until 500 ns), SER high.
    // QH shows H, G, F, E, D, C, B, A, then SER after each rising edge.
    Bench b({{"SH_LD_N", {Zero, {{500_ns, One}}}},
             {"CLK", {Zero, Clocks(9)}},
             {"SER", {One, {}}},
             {"A", {One, {}}},
             {"C", {One, {}}},
             {"D", {One, {}}},
             {"G", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const LogicValue expected[] = {Zero, One, Zero, Zero, One, One, Zero, One, One, One};
    sim.runUntil(400_ns);
    CHECK(sim.value(b.nets["QH"]) == expected[0]);
    CHECK(sim.value(b.nets["QH_N"]) == One);
    for (int i = 1; i <= 9; ++i) {
        INFO("edge " << i);
        sim.runUntil(i * 1_us + 100_ns);
        CHECK(sim.value(b.nets["QH"]) == expected[i]);
    }
}

TEST_CASE("74LS165: delays", "[parts]") {
    // H high; SH_LD_N falls at 1 µs (load), H falls at 2 µs (follows while loading),
    // SH_LD_N rises at 3 µs, CLK rises at 4 µs (QH takes G = 1).
    Bench b({{"SH_LD_N", {One, {{1_us, Zero}, {3_us, One}}}},
             {"H", {One, {{2_us, Zero}}}},
             {"G", {One, {}}},
             {"SER", {One, {}}},
             {"CLK", {Zero, {{4_us, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(2);
    sim.runUntil(1_us + delays::kLs165Load.rise);
    CHECK(sim.value(b.nets["QH"]) == One);
    sim.runUntil(2_us + delays::kLs165Data.fall - 1);
    CHECK(sim.value(b.nets["QH"]) == One);
    sim.runUntil(2_us + delays::kLs165Data.fall);
    CHECK(sim.value(b.nets["QH"]) == Zero);
    sim.runUntil(4_us + delays::kLs165Clock.rise - 1);
    CHECK(sim.value(b.nets["QH"]) == Zero);
    sim.runUntil(4_us + delays::kLs165Clock.rise);
    CHECK(sim.value(b.nets["QH"]) == One);
}

TEST_CASE("74LS165: CLK_INH and loading stop the shift", "[parts]") {
    SECTION("CLK_INH high holds") {
        Bench b({{"SH_LD_N", {Zero, {{500_ns, One}}}},
                 {"H", {One, {}}},
                 {"CLK_INH", {One, {}}},
                 {"CLK", {Zero, Clocks(3)}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(4_us);
        CHECK(sim.value(b.nets["QH"]) == One);
    }
    SECTION("CLK_INH rising while CLK is low shifts") {
        Bench b({{"SH_LD_N", {Zero, {{500_ns, One}}}},
                 {"H", {One, {}}},
                 {"CLK_INH", {Zero, {{1_us, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(900_ns);
        CHECK(sim.value(b.nets["QH"]) == One);
        sim.runUntil(1_us + delays::kLs165Clock.fall);
        CHECK(sim.value(b.nets["QH"]) == Zero);  // G was 0
    }
    SECTION("a clock edge while loading does not shift") {
        Bench b({{"SH_LD_N", {Zero, {{1_us + 100_ns, One}}}},
                 {"H", {One, {}}},
                 {"CLK", {Zero, {{1_us, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(2_us);
        CHECK(sim.value(b.nets["QH"]) == One);
    }
}

TEST_CASE("74LS165: X inputs", "[parts]") {
    SECTION("SH_LD_N at X: stages equal to their inputs stay known") {
        // Loaded with H = 1, then SH_LD_N goes X: H still 1 keeps QH; H at 0 would not.
        Bench b({{"SH_LD_N", {Zero, {{1_us, X}}}}, {"H", {One, {{2_us, Zero}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us + 500_ns);
        CHECK(sim.value(b.nets["QH"]) == One);
        sim.runUntil(3_us);
        CHECK(sim.value(b.nets["QH"]) == X);
    }
    SECTION("CLK_INH to X with CLK low") {
        // Loaded H = 1, G = 0: a possible shift makes QH unknown. G = 1: QH stays 1.
        const LogicValue g = GENERATE(Zero, One);
        Bench b({{"SH_LD_N", {Zero, {{500_ns, One}}}},
                 {"H", {One, {}}},
                 {"G", {g, {}}},
                 {"CLK_INH", {Zero, {{1_us, X}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(2_us);
        CHECK(sim.value(b.nets["QH"]) == (g == One ? One : X));
    }
}
