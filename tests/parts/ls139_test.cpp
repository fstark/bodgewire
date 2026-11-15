#include "parts/ls139.hpp"

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

// One 74LS139; every input driven by a Stimulus (both halves enabled by default), outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        for (const char* pin : {"1A", "1B", "1G_N", "2A", "2B", "2G_N"}) {
            inputs.try_emplace(pin, Input{Zero, {}});
        }
        auto& chip = circuit.add<Ls139>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (int half = 1; half <= 2; ++half) {
            for (int k = 0; k < 4; ++k) {
                const std::string pin = std::to_string(half) + "Y" + std::to_string(k) + "_N";
                nets[pin] = circuit.addNet(pin);
                circuit.connect(chip, pin, nets[pin]);
            }
        }
        circuit.finalize();
    }
};

LogicValue Bit(int value, int bit) { return (value >> bit & 1) != 0 ? One : Zero; }

std::string Y(int half, int k) { return std::to_string(half) + "Y" + std::to_string(k) + "_N"; }

}  // namespace

TEST_CASE("74LS139: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS139");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ls139 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[Ls139::k1G].name == "1G_N");
    CHECK(pins[Ls139::k1A].name == "1A");
    CHECK(pins[Ls139::k1B].name == "1B");
    CHECK(pins[3].name == "1Y0_N");
    CHECK(pins[6].name == "1Y3_N");
    CHECK(pins[7].name == "GND");
    CHECK(pins[8].name == "2Y3_N");
    CHECK(pins[11].name == "2Y0_N");
    CHECK(pins[Ls139::k2B].name == "2B");
    CHECK(pins[Ls139::k2A].name == "2A");
    CHECK(pins[Ls139::k2G].name == "2G_N");
    CHECK(pins[15].name == "VCC");
}

TEST_CASE("74LS139: each half decodes its own select", "[parts]") {
    const int one = GENERATE(0, 1, 2, 3);
    const int two = GENERATE(0, 1, 2, 3);
    INFO("half 1 " << one << ", half 2 " << two);
    Bench b({{"1A", {Bit(one, 0), {}}},
             {"1B", {Bit(one, 1), {}}},
             {"2A", {Bit(two, 0), {}}},
             {"2B", {Bit(two, 1), {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    for (int k = 0; k < 4; ++k) {
        CHECK(sim.value(b.nets[Y(1, k)]) == (k == one ? Zero : One));
        CHECK(sim.value(b.nets[Y(2, k)]) == (k == two ? Zero : One));
    }
}

TEST_CASE("74LS139: an enable off puts its half high", "[parts]") {
    // Select 2 on both halves; half 1 off from 100 ns to 200 ns.
    Bench b(
        {{"1B", {One, {}}}, {"2B", {One, {}}}, {"1G_N", {Zero, {{100_ns, One}, {200_ns, Zero}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + delays::kLs139Enable.rise - 1);
    CHECK(sim.value(b.nets["1Y2_N"]) == Zero);
    sim.runUntil(100_ns + delays::kLs139Enable.rise);
    for (int k = 0; k < 4; ++k) {
        CHECK(sim.value(b.nets[Y(1, k)]) == One);
    }
    CHECK(sim.value(b.nets["2Y2_N"]) == Zero);
    sim.runUntil(200_ns + delays::kLs139Enable.fall - 1);
    CHECK(sim.value(b.nets["1Y2_N"]) == One);
    sim.runUntil(200_ns + delays::kLs139Enable.fall);
    CHECK(sim.value(b.nets["1Y2_N"]) == Zero);
}

TEST_CASE("74LS139: select delays", "[parts]") {
    // Half 2: 0 → 3 at 100 ns.
    Bench b({{"2A", {Zero, {{100_ns, One}}}}, {"2B", {Zero, {{100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + delays::kLs139Select.rise - 1);
    CHECK(sim.value(b.nets["2Y0_N"]) == Zero);
    sim.runUntil(100_ns + delays::kLs139Select.rise);
    CHECK(sim.value(b.nets["2Y0_N"]) == One);
    sim.runUntil(100_ns + delays::kLs139Select.fall - 1);
    CHECK(sim.value(b.nets["2Y3_N"]) == One);
    sim.runUntil(100_ns + delays::kLs139Select.fall);
    CHECK(sim.value(b.nets["2Y3_N"]) == Zero);
}

TEST_CASE("74LS139: X only where it could matter", "[parts]") {
    SECTION("X on A: the two outputs it could choose are X, the others high") {
        Bench b({{"1A", {X, {}}}, {"1B", {One, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 4; ++k) {
            CHECK(sim.value(b.nets[Y(1, k)]) == (k == 2 || k == 3 ? X : One));
        }
        CHECK(sim.value(b.nets["2Y0_N"]) == Zero);
    }
    SECTION("X on the enable: the selected output X, the others high") {
        Bench b({{"2A", {One, {}}}, {"2G_N", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 4; ++k) {
            CHECK(sim.value(b.nets[Y(2, k)]) == (k == 1 ? X : One));
        }
    }
}
