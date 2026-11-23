#include "parts/ls175.hpp"

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

// One 74LS175; listed inputs driven by Stimulus, the others float high. Outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls175>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (int unit = 1; unit <= 4; ++unit) {
            for (const char* suffix : {"Q", "Q_N"}) {
                const std::string pin = std::to_string(unit) + suffix;
                nets[pin] = circuit.addNet(pin);
                circuit.connect(chip, pin, nets[pin]);
            }
        }
        circuit.finalize();
    }
};

std::string Pin(int unit, const char* suffix) { return std::to_string(unit) + suffix; }

// Clear low until 30 ns, then high.
Input ClearPulse() { return {Zero, {{30_ns, One}}}; }

}  // namespace

TEST_CASE("74LS175: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS175");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ls175 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[Ls175::kClrN].name == "CLR_N");
    CHECK(pins[Ls175::kClk].name == "CLK");
    CHECK(pins[Ls175::kClk].risingEdgeOnly);
    for (const PinSpec& pin : pins) {  // the four D inputs, nothing else
        CHECK(pin.sampled == (pin.name.size() == 2 && pin.name[1] == 'D'));
    }
    CHECK(pins[7].name == "GND");
    CHECK(pins[15].name == "VCC");
    CHECK(pins[3].name == "1D");
    CHECK(pins[14].name == "4Q");
}

TEST_CASE("74LS175: each flip-flop takes D on the rising edge only", "[parts]") {
    const int unit = GENERATE(1, 2, 3, 4);
    INFO("unit " << unit);
    // D high from 50 ns; CLK rises at 100 ns, falls at 150 ns; D low at 170 ns, CLK falls only.
    Bench b({{"CLR_N", ClearPulse()},
             {Pin(unit, "D"), {Zero, {{50_ns, One}, {170_ns, Zero}}}},
             {"CLK", {Zero, {{100_ns, One}, {150_ns, Zero}, {250_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    CHECK(sim.value(b.nets[Pin(unit, "Q_N")]) == One);
    sim.runUntil(100_ns + delays::kLs175ClockToQ.rise - 1);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    sim.runUntil(100_ns + delays::kLs175ClockToQ.rise);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == One);
    sim.runUntil(100_ns + delays::kLs175ClockToQ.fall);
    CHECK(sim.value(b.nets[Pin(unit, "Q_N")]) == Zero);
    sim.runUntil(240_ns);  // the falling edge and D falling change nothing
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == One);
    sim.runUntil(300_ns);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    CHECK(sim.value(b.nets[Pin(unit, "Q_N")]) == One);
}

TEST_CASE("74LS175: D changed at the edge is not seen", "[parts]") {
    Bench b({{"CLR_N", ClearPulse()},
             {"1D", {Zero, {{100_ns, One}}}},
             {"CLK", {Zero, {{100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(200_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
}

TEST_CASE("74LS175: four bits on one clock", "[parts]") {
    Bench b({{"CLR_N", ClearPulse()},
             {"1D", {One, {}}},
             {"2D", {Zero, {}}},
             {"3D", {Zero, {}}},
             {"4D", {One, {}}},
             {"CLK", {Zero, {{100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(3);
    sim.runUntil(200_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["2Q"]) == Zero);
    CHECK(sim.value(b.nets["3Q"]) == Zero);
    CHECK(sim.value(b.nets["4Q"]) == One);
}

TEST_CASE("74LS175: clear acts at once and wins over the clock", "[parts]") {
    // Set all four at 100 ns, clear at 200 ns, a clock edge during the clear at 250 ns.
    Bench b({{"CLR_N", {One, {{200_ns, Zero}}}},
             {"1D", {One, {}}},
             {"2D", {One, {}}},
             {"3D", {One, {}}},
             {"4D", {One, {}}},
             {"CLK", {Zero, {{100_ns, One}, {150_ns, Zero}, {250_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(190_ns);
    CHECK(sim.value(b.nets["3Q"]) == One);
    sim.runUntil(200_ns + delays::kLs175Clear.rise);
    CHECK(sim.value(b.nets["3Q_N"]) == One);
    CHECK(sim.value(b.nets["3Q"]) == One);
    sim.runUntil(200_ns + delays::kLs175Clear.fall);
    for (int unit = 1; unit <= 4; ++unit) {
        CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    }
    sim.runUntil(350_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
}

TEST_CASE("74LS175: X on D, CLR_N and CLK", "[parts]") {
    SECTION("D at X clocked gives X; a known D recovers") {
        Bench b({{"CLR_N", ClearPulse()},
                 {"1D", {X, {{150_ns, One}}}},
                 {"CLK", {Zero, {{100_ns, One}, {130_ns, Zero}, {200_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(140_ns);
        CHECK(sim.value(b.nets["1Q"]) == X);
        CHECK(sim.value(b.nets["1Q_N"]) == X);
        sim.runUntil(250_ns);
        CHECK(sim.value(b.nets["1Q"]) == One);
    }
    SECTION("CLR_N at X gives X") {
        Bench b({{"CLR_N", {One, {{100_ns, X}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(200_ns);
        CHECK(sim.value(b.nets["2Q"]) == X);
    }
    SECTION("X to 1 on CLK takes nothing") {
        Bench b({{"CLR_N", ClearPulse()},
                 {"1D", {One, {}}},
                 {"CLK", {Zero, {{100_ns, X}, {150_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(250_ns);
        CHECK(sim.value(b.nets["1Q"]) == Zero);
    }
}
