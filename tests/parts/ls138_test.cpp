#include "parts/ls138.hpp"

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

// One 74LS138; every input driven by a Stimulus (enabled by default), outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        for (const auto& [pin, level] :
             {std::pair{"A", Zero}, std::pair{"B", Zero}, std::pair{"C", Zero},
              std::pair{"G1", One}, std::pair{"G2A_N", Zero}, std::pair{"G2B_N", Zero}}) {
            inputs.try_emplace(pin, Input{level, {}});
        }
        auto& chip = circuit.add<Ls138>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (int k = 0; k < 8; ++k) {
            const std::string pin = "Y" + std::to_string(k) + "_N";
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

LogicValue Bit(int value, int bit) { return (value >> bit & 1) != 0 ? One : Zero; }

std::string Y(int k) { return "Y" + std::to_string(k) + "_N"; }

}  // namespace

TEST_CASE("74LS138: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS138");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ls138 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[Ls138::kA].name == "A");
    CHECK(pins[Ls138::kC].name == "C");
    CHECK(pins[Ls138::kG1].name == "G1");
    CHECK(pins[6].name == "Y7_N");
    CHECK(pins[7].name == "GND");
    CHECK(pins[14].name == "Y0_N");
    CHECK(pins[15].name == "VCC");
    for (const PinSpec& pin : pins) {
        CHECK_FALSE(pin.sampled);
    }
}

TEST_CASE("74LS138: each select value pulls its own output low", "[parts]") {
    const int value = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
    INFO("select " << value);
    Bench b({{"A", {Bit(value, 0), {}}}, {"B", {Bit(value, 1), {}}}, {"C", {Bit(value, 2), {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    for (int k = 0; k < 8; ++k) {
        CHECK(sim.value(b.nets[Y(k)]) == (k == value ? Zero : One));
    }
}

TEST_CASE("74LS138: any enable off puts every output high", "[parts]") {
    const auto [pin, off] =
        GENERATE(std::pair{"G1", Zero}, std::pair{"G2A_N", One}, std::pair{"G2B_N", One});
    INFO(pin);
    // Select 5; the enable goes off at 100 ns and back on at 200 ns.
    const LogicValue on = off == Zero ? One : Zero;
    Bench b({{"A", {One, {}}}, {"C", {One, {}}}, {pin, {on, {{100_ns, off}, {200_ns, on}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["Y5_N"]) == Zero);
    const Delays enable =
        std::string(pin) == "G1" ? delays::kLs138EnableG1 : delays::kLs138EnableG2;
    sim.runUntil(100_ns + enable.rise - 1);
    CHECK(sim.value(b.nets["Y5_N"]) == Zero);
    sim.runUntil(100_ns + enable.rise);
    for (int k = 0; k < 8; ++k) {
        CHECK(sim.value(b.nets[Y(k)]) == One);
    }
    sim.runUntil(200_ns + enable.fall - 1);
    CHECK(sim.value(b.nets["Y5_N"]) == One);
    sim.runUntil(200_ns + enable.fall);
    CHECK(sim.value(b.nets["Y5_N"]) == Zero);
}

TEST_CASE("74LS138: select delays", "[parts]") {
    // 0 → 3 at 100 ns.
    Bench b({{"A", {Zero, {{100_ns, One}}}}, {"B", {Zero, {{100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + delays::kLs138Select.fall - 1);
    CHECK(sim.value(b.nets["Y3_N"]) == One);
    CHECK(sim.value(b.nets["Y0_N"]) == Zero);
    sim.runUntil(100_ns + delays::kLs138Select.fall);
    CHECK(sim.value(b.nets["Y3_N"]) == Zero);
    sim.runUntil(100_ns + delays::kLs138Select.rise);
    CHECK(sim.value(b.nets["Y0_N"]) == One);
}

TEST_CASE("74LS138: X only where it could matter", "[parts]") {
    SECTION("X on A: the two outputs it could choose are X, the others high") {
        Bench b({{"A", {X, {}}}, {"C", {One, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 8; ++k) {
            CHECK(sim.value(b.nets[Y(k)]) == (k == 4 || k == 5 ? X : One));
        }
    }
    SECTION("X on an enable: the selected output X, the others high") {
        Bench b({{"B", {One, {}}}, {"G2B_N", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 8; ++k) {
            CHECK(sim.value(b.nets[Y(k)]) == (k == 2 ? X : One));
        }
    }
    SECTION("X on an enable while another is off: all high") {
        Bench b({{"G1", {Zero, {}}}, {"G2A_N", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        for (int k = 0; k < 8; ++k) {
            CHECK(sim.value(b.nets[Y(k)]) == One);
        }
    }
}
