// The resistor network (P5u): series resistors between signal Nets, resolved by group.

#include "parts/resistor_network.hpp"

#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/switch.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;

namespace {

using enum LogicValue;

// Nets N0, N1, … joined by the edges given, each Net optionally driven by a Stimulus.
struct Bench {
    Circuit circuit;
    std::vector<NetId> nets;
    ResistorNetwork* network = nullptr;

    explicit Bench(std::size_t count) {
        std::vector<std::string> names;
        for (std::size_t i = 0; i < count; ++i) {
            names.push_back("N" + std::to_string(i));
            nets.push_back(circuit.addNet(names.back()));
        }
        network = &circuit.add<ResistorNetwork>(names);
        for (PinIndex p = 0; p < count; ++p) {
            circuit.connect(*network, p, nets[p]);
        }
    }
    void drive(std::size_t net, LogicValue initial, std::vector<Stimulus::Step> steps = {},
               Family family = Family::Ttl) {
        auto& s =
            circuit.add<Stimulus>("drv" + std::to_string(net), initial, std::move(steps), family);
        circuit.connect(s, Stimulus::kOut, nets[net]);
    }
    Simulator& start() {
        circuit.finalize();
        sim = std::make_unique<Simulator>(circuit);
        sim->powerOn(1);
        sim->runFor(10);
        return *sim;
    }
    LogicValue value(std::size_t net) const { return sim->value(nets[net]); }
    bool floating(std::size_t net) const { return sim->isFloating(nets[net]); }
    std::unique_ptr<Simulator> sim;
};

}  // namespace

TEST_CASE("resistor network: an undriven Net takes the other side's value", "[parts]") {
    Bench bench(2);
    bench.network->join(0, 1, 1000);
    bench.drive(0, Zero, {{1000, One}, {2000, X}});
    Simulator& sim = bench.start();
    CHECK(bench.value(1) == Zero);
    CHECK_FALSE(bench.floating(1));
    sim.runUntil(1000);
    CHECK(bench.value(1) == Zero);  // follows 1 ps later
    sim.runUntil(1001);
    CHECK(bench.value(1) == One);
    sim.runUntil(2001);
    CHECK(bench.value(1) == X);
}

TEST_CASE("resistor network: a driven Net keeps its own value", "[parts]") {
    SECTION("two outputs") {
        Bench bench(2);
        bench.network->join(0, 1, 1000);
        bench.drive(0, One);
        bench.drive(1, Zero, {}, Family::Mos);
        bench.start();
        CHECK(bench.value(0) == One);
        CHECK(bench.value(1) == Zero);
    }
    SECTION("an open-collector 0 on one side, an output's 1 on the other (the ZX80's NOP)") {
        Bench bench(2);
        bench.network->join(0, 1, 1000);
        bench.circuit.tie(bench.nets[0], Zero, DriveStrength::Strong);
        bench.drive(1, One, {}, Family::Mos);
        bench.start();
        CHECK(bench.value(0) == Zero);
        CHECK(bench.value(1) == One);
    }
}

TEST_CASE("resistor network: less resistance wins between weak values", "[parts]") {
    SECTION("a pull-up beyond the 47K loses to a driven value through 1K") {
        Bench bench(2);
        bench.network->join(0, 1, 1000);
        bench.network->pull(1, One, 47000);
        bench.drive(0, Zero, {{1000, Z}});
        Simulator& sim = bench.start();
        CHECK(bench.value(1) == Zero);
        sim.runUntil(1010);  // nothing drives N0 now: the pull-up holds both
        CHECK(bench.value(1) == One);
        CHECK(bench.value(0) == One);
        CHECK_FALSE(bench.floating(0));
    }
    SECTION("equal paths that disagree give X") {
        Bench bench(3);
        bench.network->join(0, 1, 1000);
        bench.network->join(1, 2, 1000);
        bench.drive(0, Zero);
        bench.drive(2, One);
        bench.start();
        CHECK(bench.value(1) == X);
    }
    SECTION("the nearer one wins") {
        Bench bench(3);
        bench.network->join(0, 1, 1000);
        bench.network->join(1, 2, 2200);
        bench.drive(0, Zero);
        bench.drive(2, One);
        bench.start();
        CHECK(bench.value(1) == Zero);
    }
    SECTION("two pulls on one Net that disagree at the same ohms") {
        Bench bench(2);
        bench.network->join(0, 1, 1000);
        bench.network->pull(1, One, 10000);
        bench.network->pull(1, Zero, 10000);
        bench.start();
        CHECK(bench.value(0) == X);
    }
}

TEST_CASE("resistor network: a chain of three Nets", "[parts]") {
    Bench bench(3);
    bench.network->join(0, 1, 1000);
    bench.network->join(1, 2, 1000);
    bench.drive(2, One, {{1000, Zero}});
    Simulator& sim = bench.start();
    CHECK(bench.value(0) == One);
    sim.runUntil(1001);
    CHECK(bench.value(0) == Zero);
}

TEST_CASE("resistor network: no driver anywhere and every Net floats", "[parts]") {
    Bench bench(2);
    bench.network->join(0, 1, 1000);
    bench.start();
    CHECK(bench.floating(0));
    CHECK(bench.floating(1));
}

TEST_CASE("resistor network: two groups resolve apart", "[parts]") {
    Bench bench(4);
    bench.network->join(0, 1, 1000);
    bench.network->join(2, 3, 1000);
    bench.drive(0, One);
    bench.drive(2, Zero);
    bench.start();
    CHECK(bench.value(1) == One);
    CHECK(bench.value(3) == Zero);
}

TEST_CASE("resistor network: built from a netlist with the pulls of joined Nets", "[parts]") {
    // U1's first inverter drives A low; R1 joins B, which R2 pulls up through 47K: B reads A's 0
    // through 1K. Were R2 tied as a plain pull-up, B would count as driven and read 1.
    const auto loaded = LoadNetlist("part J1 POWER\npart U1 74LS04\npart R1 R value=1k\n"
                                    "part R2 R value=47k\nnet VCC U1.1A R2.1\n"
                                    "net A U1.1Y R1.1\nnet B R1.2 R2.2 U1.3A\n"
                                    "net GND U1.4A U1.5A U1.6A\n",
                                    "t.bwn");
    REQUIRE(loaded.has_value());
    Circuit& circuit = (*loaded)->circuit;
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runFor(1'000'000);
    CHECK(sim.value((*loaded)->nets.at("A")) == Zero);
    CHECK(sim.value((*loaded)->nets.at("B")) == Zero);
    CHECK_FALSE(sim.isFloating((*loaded)->nets.at("B")));
}

TEST_CASE("resistor network: a key on a joined Net", "[parts]") {
    // U1's first inverter drives A low; K1 joins A to B while pressed; R1 joins B to C, which R2
    // pulls up. Key up: B and C read R2's 1. Key down: A's 0 reaches B through the key and C
    // through R1 (1k beats 47k). The Net B has the switch network's Pin and the resistor
    // network's.
    const auto loaded = LoadNetlist("part J1 POWER\npart U1 74LS04\npart K1 KEY legend=1\n"
                                    "part R1 R value=1k\npart R2 R value=47k\n"
                                    "net VCC U1.1A R2.1\nnet A U1.1Y K1.1\nnet B K1.2 R1.1\n"
                                    "net C R1.2 R2.2 U1.2A\nnet GND U1.3A U1.4A U1.5A U1.6A\n",
                                    "t.bwn");
    REQUIRE(loaded.has_value());
    Simulator sim((*loaded)->circuit);
    sim.powerOn(1);
    sim.runFor(1'000'000);
    const auto value = [&](const char* net) { return sim.value((*loaded)->nets.at(net)); };
    CHECK(value("A") == Zero);
    CHECK(value("B") == One);
    CHECK(value("C") == One);
    Switch* key = SwitchOf(**loaded, "K1");
    REQUIRE(key != nullptr);
    key->setPressed(true);
    sim.wake(key->id());
    sim.runFor(1'000'000);
    CHECK(value("B") == Zero);
    CHECK(value("C") == Zero);
    key->setPressed(false);
    sim.wake(key->id());
    sim.runFor(1'000'000);
    CHECK(value("B") == One);
    CHECK(value("C") == One);
}
