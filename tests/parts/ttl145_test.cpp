#include "parts/ttl145.hpp"

#include <map>
#include <string>
#include <string_view>
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

// One 74145; every input driven by a Stimulus (0 by default); every output pulled up (open
// collector), on its own Net.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs, std::string_view part = "74145") {
        for (const char* pin : {"A", "B", "C", "D"}) {
            inputs.try_emplace(pin, Input{Zero, {}});
        }
        auto& chip = *FindPart(part)->create(circuit, "U1", {}).value();
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
        }
        for (int k = 0; k < 10; ++k) {
            const std::string pin = "Y" + std::to_string(k) + "_N";
            nets[pin] = circuit.addNet(pin);
            circuit.tie(nets[pin], One, DriveStrength::Weak);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

LogicValue Bit(int value, int bit) { return (value >> bit & 1) != 0 ? One : Zero; }

std::string Y(int k) { return "Y" + std::to_string(k) + "_N"; }

}  // namespace

TEST_CASE("74145: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74145");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);
    Ttl145 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    CHECK(pins[0].name == "Y0_N");
    CHECK(pins[6].name == "Y6_N");
    CHECK(pins[7].name == "GND");
    CHECK(pins[8].name == "Y7_N");
    CHECK(pins[10].name == "Y9_N");
    CHECK(pins[Ttl145::kD].name == "D");
    CHECK(pins[Ttl145::kA].name == "A");
    CHECK(pins[15].name == "VCC");
    for (const PinSpec& pin : pins) {
        CHECK_FALSE(pin.sampled);
        if (pin.name.starts_with("Y")) {
            CHECK(pin.kind == PinKind::OpenCollector);
        }
    }
}

TEST_CASE("74145: each code pulls its output low, 10-15 none", "[parts]") {
    const std::string_view part = GENERATE("74145", "74LS145");
    INFO(part);
    for (int code = 0; code < 16; ++code) {
        Bench bench({{"A", {Bit(code, 0), {}}},
                     {"B", {Bit(code, 1), {}}},
                     {"C", {Bit(code, 2), {}}},
                     {"D", {Bit(code, 3), {}}}},
                    part);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        for (int k = 0; k < 10; ++k) {
            INFO("code " << code << " output " << k);
            CHECK(sim.value(bench.nets[Y(k)]) == (k == code ? Zero : One));
            CHECK(sim.isFloating(bench.nets[Y(k)]) == false);
        }
    }
}

TEST_CASE("74145: an X input makes the outputs it could select X", "[parts]") {
    // D = X, C B A = 0: code 0 or 8.
    Bench bench({{"D", {X, {}}}});
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    for (int k = 0; k < 10; ++k) {
        CHECK(sim.value(bench.nets[Y(k)]) == (k == 0 || k == 8 ? X : One));
    }
}

TEST_CASE("74145: delays", "[parts]") {
    // 3 → 4 at 1 µs: Y3_N released after the rise delay, Y4_N pulled after the fall delay. The
    // 74LS145 is the same chip with its own delays.
    const auto [part, delays] =
        GENERATE(std::pair{"74145", delays::kTtl145}, std::pair{"74LS145", delays::kLs145});
    INFO(part);
    Bench bench(
        {{"A", {One, {{1_us, Zero}}}}, {"B", {One, {{1_us, Zero}}}}, {"C", {Zero, {{1_us, One}}}}},
        part);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + delays.fall - 1);
    CHECK(sim.value(bench.nets[Y(4)]) == One);
    sim.runUntil(1_us + delays.fall);
    CHECK(sim.value(bench.nets[Y(4)]) == Zero);
    sim.runUntil(1_us + delays.rise - 1);
    CHECK(sim.value(bench.nets[Y(3)]) == Zero);
    sim.runUntil(1_us + delays.rise);
    CHECK(sim.value(bench.nets[Y(3)]) == One);
}
