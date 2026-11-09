#include "parts/ls157.hpp"

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

// One 74LS157; listed inputs driven by Stimulus, the others float high. Outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls157>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (int unit = 1; unit <= 4; ++unit) {
            const std::string pin = std::to_string(unit) + "Y";
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

std::string Pin(int unit, const char* suffix) { return std::to_string(unit) + suffix; }

}  // namespace

TEST_CASE("74LS157: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS157");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ls157 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[Ls157::kS].name == "S");
    CHECK(pins[Ls157::kGN].name == "G_N");
    CHECK(pins[7].name == "GND");
    CHECK(pins[15].name == "VCC");
    CHECK(pins[1].name == "1A");
    CHECK(pins[9].name == "3B");
    CHECK(pins[10].name == "3A");
    CHECK(pins[11].name == "4Y");
    CHECK(pins[11].kind == PinKind::Output);
}

TEST_CASE("74LS157: each Y follows A while S is low, B while S is high", "[parts]") {
    const int unit = GENERATE(1, 2, 3, 4);
    INFO("unit " << unit);
    // A high, B low; S rises at 100 ns; A falls at 200 ns (unseen); B rises at 300 ns.
    Bench b({{"S", {Zero, {{100_ns, One}}}},
             {"G_N", {Zero, {}}},
             {Pin(unit, "A"), {One, {{200_ns, Zero}}}},
             {Pin(unit, "B"), {Zero, {{300_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == One);
    sim.runUntil(100_ns + delays::kLs157Select.fall - 1);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == One);
    sim.runUntil(100_ns + delays::kLs157Select.fall);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == Zero);
    sim.runUntil(290_ns);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == Zero);
    sim.runUntil(300_ns + delays::kLs157Data.rise - 1);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == Zero);
    sim.runUntil(300_ns + delays::kLs157Data.rise);
    CHECK(sim.value(b.nets[Pin(unit, "Y")]) == One);
}

TEST_CASE("74LS157: G_N high forces every Y low", "[parts]") {
    Bench b({{"S", {One, {}}},
             {"G_N", {Zero, {{100_ns, One}, {200_ns, Zero}}}},
             {"1B", {One, {}}},
             {"2B", {One, {}}},
             {"3B", {One, {}}},
             {"4B", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["3Y"]) == One);
    sim.runUntil(100_ns + delays::kLs157Strobe.fall - 1);
    CHECK(sim.value(b.nets["3Y"]) == One);
    sim.runUntil(100_ns + delays::kLs157Strobe.fall);
    for (int unit = 1; unit <= 4; ++unit) {
        CHECK(sim.value(b.nets[Pin(unit, "Y")]) == Zero);
    }
    sim.runUntil(200_ns + delays::kLs157Strobe.rise);
    CHECK(sim.value(b.nets["1Y"]) == One);
}

TEST_CASE("74LS157: X on S and G_N", "[parts]") {
    SECTION("S at X: Y is A when A = B, X otherwise") {
        Bench b({{"S", {X, {}}},
                 {"G_N", {Zero, {}}},
                 {"1A", {One, {}}},
                 {"1B", {One, {}}},
                 {"2A", {Zero, {}}},
                 {"2B", {Zero, {}}},
                 {"3A", {One, {}}},
                 {"3B", {Zero, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(sim.value(b.nets["1Y"]) == One);
        CHECK(sim.value(b.nets["2Y"]) == Zero);
        CHECK(sim.value(b.nets["3Y"]) == X);
    }
    SECTION("G_N at X: 0 when the selected data is 0, X otherwise") {
        Bench b({{"S", {Zero, {}}}, {"G_N", {X, {}}}, {"1A", {Zero, {}}}, {"2A", {One, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(sim.value(b.nets["1Y"]) == Zero);
        CHECK(sim.value(b.nets["2Y"]) == X);
    }
    SECTION("an X data input gives X when selected only") {
        Bench b({{"S", {One, {}}}, {"G_N", {Zero, {}}}, {"1A", {X, {}}}, {"1B", {Zero, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(sim.value(b.nets["1Y"]) == Zero);
    }
}
