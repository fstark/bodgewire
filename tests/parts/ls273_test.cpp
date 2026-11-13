#include "parts/ls273.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One 74LS273; listed inputs driven by Stimulus, the others float high. Outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls273>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (int unit = 1; unit <= 8; ++unit) {
            const std::string pin = std::to_string(unit) + "Q";
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

std::string Pin(int unit, const char* suffix) { return std::to_string(unit) + suffix; }

Input ClearPulse() { return {Zero, {{30_ns, One}}}; }  // low until 30 ns

}  // namespace

TEST_CASE("74LS273: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS273");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-20");
    CHECK(part->kind == PartKind::Chip);
    Ls273 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 20);
    CHECK(pins[Ls273::kClrN].name == "CLR_N");
    CHECK(pins[Ls273::kClk].name == "CLK");
    CHECK(pins[Ls273::kClk].risingEdgeOnly);
    for (const PinSpec& pin : pins) {  // the eight D inputs, nothing else
        CHECK(pin.sampled == (pin.name.size() == 2 && pin.name[1] == 'D'));
    }
    CHECK(pins[1].name == "1Q");
    CHECK(pins[2].name == "1D");
    CHECK(pins[9].name == "GND");
    CHECK(pins[17].name == "8D");
    CHECK(pins[18].name == "8Q");
    CHECK(pins[19].name == "VCC");
}

TEST_CASE("74LS273: each flip-flop takes D on the rising edge only", "[parts]") {
    const int unit = GENERATE(1, 2, 3, 4, 5, 6, 7, 8);
    INFO("unit " << unit);
    // D high from 50 ns; CLK rises at 100 ns, falls at 150 ns; D low at 170 ns, CLK rises at 250.
    Bench b({{"CLR_N", ClearPulse()},
             {Pin(unit, "D"), {Zero, {{50_ns, One}, {170_ns, Zero}}}},
             {"CLK", {Zero, {{100_ns, One}, {150_ns, Zero}, {250_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    sim.runUntil(100_ns + delays::kLs273ClockToQ.rise - 1);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    sim.runUntil(100_ns + delays::kLs273ClockToQ.rise);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == One);
    sim.runUntil(240_ns);  // the falling edge and D falling change nothing
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == One);
    sim.runUntil(250_ns + delays::kLs273ClockToQ.fall);
    CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
}

TEST_CASE("74LS273: a byte on one clock, D changed at the edge not seen", "[parts]") {
    std::map<std::string, Input> inputs{{"CLR_N", ClearPulse()}, {"CLK", {Zero, {{100_ns, One}}}}};
    for (int unit = 1; unit <= 8; ++unit) {  // $A5, every D flipping at the edge
        const LogicValue bit = (0xA5 >> (unit - 1) & 1) != 0 ? One : Zero;
        inputs[Pin(unit, "D")] = {bit, {{100_ns, bit == One ? Zero : One}}};
    }
    Bench b(std::move(inputs));
    Simulator sim(b.circuit);
    sim.powerOn(2);
    sim.runUntil(200_ns);
    for (int unit = 1; unit <= 8; ++unit) {
        CHECK(sim.value(b.nets[Pin(unit, "Q")]) == ((0xA5 >> (unit - 1) & 1) != 0 ? One : Zero));
    }
}

TEST_CASE("74LS273: clear acts at once and wins over the clock", "[parts]") {
    Bench b({{"CLR_N", {One, {{200_ns, Zero}}}},
             {"3D", {One, {}}},
             {"8D", {One, {}}},
             {"CLK", {Zero, {{100_ns, One}, {150_ns, Zero}, {250_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(190_ns);
    CHECK(sim.value(b.nets["3Q"]) == One);
    CHECK(sim.value(b.nets["8Q"]) == One);
    sim.runUntil(200_ns + delays::kLs273Clear.fall - 1);
    CHECK(sim.value(b.nets["3Q"]) == One);
    sim.runUntil(200_ns + delays::kLs273Clear.fall);
    for (int unit = 1; unit <= 8; ++unit) {
        CHECK(sim.value(b.nets[Pin(unit, "Q")]) == Zero);
    }
    sim.runUntil(350_ns);
    CHECK(sim.value(b.nets["8Q"]) == Zero);
}

TEST_CASE("74LS273: X on D and CLR_N", "[parts]") {
    SECTION("D at X clocked gives X; a known D recovers") {
        Bench b({{"CLR_N", ClearPulse()},
                 {"5D", {X, {{150_ns, One}}}},
                 {"CLK", {Zero, {{100_ns, One}, {130_ns, Zero}, {200_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(140_ns);
        CHECK(sim.value(b.nets["5Q"]) == X);
        sim.runUntil(250_ns);
        CHECK(sim.value(b.nets["5Q"]) == One);
    }
    SECTION("CLR_N at X gives X") {
        Bench b({{"CLR_N", {One, {{100_ns, X}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(200_ns);
        CHECK(sim.value(b.nets["6Q"]) == X);
    }
}
