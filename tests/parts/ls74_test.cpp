#include "parts/ls74.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

struct Drive74 {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One 74LS74; listed inputs driven by Stimulus, the others float high. Outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Drive74> inputs) {
        auto& chip = circuit.add<Ls74>("U1");
        for (auto& [pin, drive] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, drive.initial, std::move(drive.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : {"1Q", "1Q_N", "2Q", "2Q_N"}) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("74LS74: captures D on rising CLK", "[parts]") {
    Bench b({{"1D", {Zero, {{50_ns, One}}}},
             {"1CLK", {Zero, {{20_ns, One}, {70_ns, Zero}, {100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
    CHECK(sim.value(b.nets["1Q_N"]) == One);
    sim.runUntil(140_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["1Q_N"]) == Zero);
}

TEST_CASE("74LS74: ignores falling CLK", "[parts]") {
    Bench b({{"1D", {One, {{80_ns, Zero}}}}, {"1CLK", {Zero, {{50_ns, One}, {100_ns, Zero}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.runUntil(150_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
}

TEST_CASE("74LS74: preset sets, clear resets, both low gives both high", "[parts]") {
    Bench b({{"1PRE_N", {One, {{50_ns, Zero}, {100_ns, One}, {250_ns, Zero}}}},
             {"1CLR_N", {One, {{150_ns, Zero}, {200_ns, One}, {250_ns, Zero}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["1Q_N"]) == Zero);
    sim.runUntil(140_ns);  // preset released: holds
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.runUntil(190_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
    CHECK(sim.value(b.nets["1Q_N"]) == One);
    sim.runUntil(290_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["1Q_N"]) == One);
}

TEST_CASE("74LS74: async preset overrides clock", "[parts]") {
    Bench b({{"1D", {Zero, {}}},
             {"1PRE_N", {One, {{50_ns, Zero}, {150_ns, One}}}},
             {"1CLK", {Zero, {{100_ns, One}, {170_ns, Zero}, {200_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(140_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.runUntil(190_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.runUntil(240_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
}

TEST_CASE("74LS74: D changing with CLK samples old D", "[parts]") {
    Bench b(
        {{"1D", {Zero, {{100_ns, One}, {200_ns, Zero}}}},
         {"1CLK",
          {Zero, {{20_ns, One}, {60_ns, Zero}, {100_ns, One}, {150_ns, Zero}, {200_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(140_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
    sim.runUntil(240_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
}

TEST_CASE("74LS74: units are independent", "[parts]") {
    Bench b({{"1D", {One, {}}},
             {"2D", {Zero, {}}},
             {"1CLK", {Zero, {{50_ns, One}}}},
             {"2CLK", {Zero, {{100_ns, One}}}},
             {"2PRE_N", {One, {{150_ns, Zero}}}},
             {"1CLR_N", {One, {{200_ns, Zero}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.runUntil(140_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["2Q"]) == Zero);
    sim.runUntil(190_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["2Q"]) == One);
    sim.runUntil(240_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
    CHECK(sim.value(b.nets["2Q"]) == One);
}

TEST_CASE("74LS74: delays", "[parts]") {
    Bench b(
        {{"1D", {Zero, {{50_ns, One}, {150_ns, Zero}}}},
         {"1CLK",
          {Zero, {{20_ns, One}, {60_ns, Zero}, {100_ns, One}, {150_ns, Zero}, {200_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    sim.record(b.nets["1Q"]);
    sim.record(b.nets["1Q_N"]);
    sim.runUntil(300_ns);
    const auto q = sim.recorder().samples(b.nets["1Q"]);
    const auto qn = sim.recorder().samples(b.nets["1Q_N"]);
    REQUIRE(q.size() == 3);
    REQUIRE(qn.size() == 3);
    CHECK((q[1] == Sample{100_ns + delays::kLs74.rise, One}));
    CHECK((q[2] == Sample{200_ns + delays::kLs74.fall, Zero}));
    CHECK((qn[1] == Sample{100_ns + delays::kLs74.fall, Zero}));
    CHECK((qn[2] == Sample{200_ns + delays::kLs74.rise, One}));
}
