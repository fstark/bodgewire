#include "parts/ls161.hpp"

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

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// Rising edges at 100 ns, 200 ns, ... (count of them), falling 50 ns after each.
Input Clock(int rises) {
    Input clock{Zero, {}};
    for (int k = 1; k <= rises; ++k) {
        clock.steps.push_back({k * 100_ns, One});
        clock.steps.push_back({k * 100_ns + 50_ns, Zero});
    }
    return clock;
}

// Clear until 50 ns, then high, plus extra steps.
Input ClearN(std::vector<Stimulus::Step> later = {}) {
    Input clear{Zero, {{50_ns, One}}};
    clear.steps.insert(clear.steps.end(), later.begin(), later.end());
    return clear;
}

// One 74LS161; listed inputs driven by Stimulus, the others float high.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls161>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : {"QA", "QB", "QC", "QD", "RCO"}) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }

    // -1 if any output is X.
    int count(const Simulator& sim) {
        int value = 0;
        int bit = 0;
        for (const char* pin : {"QA", "QB", "QC", "QD"}) {
            const LogicValue v = sim.value(nets[pin]);
            if (v == X) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << bit++;
        }
        return value;
    }
};

// Settled sample time after the k-th rising edge of Clock().
Time After(int k) { return k * 100_ns + 60_ns; }

}  // namespace

TEST_CASE("74LS161: counts 0 to 15 and wraps", "[parts]") {
    Bench b({{"CLK", Clock(18)}, {"CLR_N", ClearN()}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    for (int k = 0; k <= 18; ++k) {
        sim.runUntil(After(k));
        CHECK(b.count(sim) == k % 16);
    }
}

TEST_CASE("74LS161: RCO high only at 15 with ENT high", "[parts]") {
    Bench b({{"CLK", Clock(18)}, {"CLR_N", ClearN()}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    for (int k = 0; k <= 18; ++k) {
        sim.runUntil(After(k));
        CHECK(sim.value(b.nets["RCO"]) == (k % 16 == 15 ? One : Zero));
    }
}

TEST_CASE("74LS161: ENP low holds", "[parts]") {
    Bench b(
        {{"CLK", Clock(7)}, {"CLR_N", ClearN()}, {"ENP", {One, {{350_ns, Zero}, {650_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(3));
    CHECK(b.count(sim) == 3);
    sim.runUntil(After(6));
    CHECK(b.count(sim) == 3);
    sim.runUntil(After(7));
    CHECK(b.count(sim) == 4);
}

TEST_CASE("74LS161: ENT low holds and forces RCO low", "[parts]") {
    Bench b({{"CLK", Clock(17)},
             {"CLR_N", ClearN()},
             {"ENT", {One, {{1580_ns, Zero}, {1680_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(15));
    CHECK(b.count(sim) == 15);
    CHECK(sim.value(b.nets["RCO"]) == One);
    sim.runUntil(After(16));
    CHECK(b.count(sim) == 15);
    CHECK(sim.value(b.nets["RCO"]) == Zero);
    sim.runUntil(1695_ns);
    CHECK(sim.value(b.nets["RCO"]) == One);
    sim.runUntil(After(17));
    CHECK(b.count(sim) == 0);
}

TEST_CASE("74LS161: synchronous load", "[parts]") {
    Bench b({{"CLK", Clock(4)},
             {"CLR_N", ClearN()},
             {"LOAD_N", {One, {{250_ns, Zero}, {350_ns, One}}}},
             {"A", {Zero, {}}},
             {"B", {One, {}}},
             {"C", {Zero, {}}},
             {"D", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(260_ns);
    CHECK(b.count(sim) == 2);
    sim.runUntil(After(3));
    CHECK(b.count(sim) == 10);
    sim.runUntil(After(4));
    CHECK(b.count(sim) == 11);
}

TEST_CASE("74LS161: asynchronous clear between edges", "[parts]") {
    Bench b({{"CLK", Clock(7)}, {"CLR_N", ClearN({{560_ns, Zero}, {620_ns, One}})}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(5));
    CHECK(b.count(sim) == 5);
    sim.runUntil(590_ns);
    CHECK(b.count(sim) == 0);
    sim.runUntil(After(6));
    CHECK(b.count(sim) == 0);
    sim.runUntil(After(7));
    CHECK(b.count(sim) == 1);
}

TEST_CASE("74LS161: X on LOAD_N gives X outputs until clear", "[parts]") {
    Bench b({{"CLK", Clock(7)},
             {"CLR_N", ClearN({{580_ns, Zero}, {620_ns, One}})},
             {"LOAD_N", {One, {{250_ns, X}, {350_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(2));
    CHECK(b.count(sim) == 2);
    sim.runUntil(After(3));
    CHECK(b.count(sim) == -1);
    sim.runUntil(After(5));
    CHECK(b.count(sim) == -1);
    CHECK(sim.value(b.nets["RCO"]) == X);
    sim.runUntil(610_ns);
    CHECK(b.count(sim) == 0);
    sim.runUntil(After(7));
    CHECK(b.count(sim) == 1);
}

TEST_CASE("74LS161: two cascaded count to 255", "[parts]") {
    Circuit circuit;
    const NetId clk = circuit.addNet("CLK");
    const NetId clr = circuit.addNet("CLR_N");
    const NetId carry = circuit.addNet("CARRY");
    Input clock = Clock(257);
    auto& sclk = circuit.add<Stimulus>("SCLK", clock.initial, std::move(clock.steps));
    auto& sclr = circuit.add<Stimulus>("SCLR", Zero, std::vector<Stimulus::Step>{{50_ns, One}});
    auto& u1 = circuit.add<Ls161>("U1");
    auto& u2 = circuit.add<Ls161>("U2");
    circuit.connect(sclk, Stimulus::kOut, clk);
    circuit.connect(sclr, Stimulus::kOut, clr);
    std::vector<NetId> q;
    for (Ls161* chip : {&u1, &u2}) {
        circuit.connect(*chip, Ls161::kClk, clk);
        circuit.connect(*chip, Ls161::kClrN, clr);
        for (PinIndex pin : {Ls161::kQA, Ls161::kQB, Ls161::kQC, Ls161::kQD}) {
            q.push_back(circuit.addNet(std::string(chip->name()) + "." + chip->pins()[pin].name));
            circuit.connect(*chip, pin, q.back());
        }
    }
    circuit.connect(u1, Ls161::kRco, carry);
    circuit.connect(u2, Ls161::kEnt, carry);
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    for (int k = 0; k <= 257; ++k) {
        sim.runUntil(After(k));
        int value = 0;
        for (std::size_t bit = 0; bit < q.size(); ++bit) {
            value |= (sim.value(q[bit]) == One ? 1 : 0) << bit;
        }
        CHECK(value == k % 256);
    }
}

TEST_CASE("74LS161: delays", "[parts]") {
    SECTION("CLK to Q and CLR_N to Q") {
        Bench b({{"CLK", Clock(3)}, {"CLR_N", ClearN({{350_ns, Zero}})}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(After(0));
        sim.record(b.nets["QA"]);
        sim.runUntil(500_ns);
        const auto qa = sim.recorder().samples(b.nets["QA"]);
        REQUIRE(qa.size() == 5);
        CHECK((qa[1] == Sample{100_ns + delays::kLs161ClockToQ.rise, One}));
        CHECK((qa[2] == Sample{200_ns + delays::kLs161ClockToQ.fall, Zero}));
        CHECK((qa[3] == Sample{300_ns + delays::kLs161ClockToQ.rise, One}));
        CHECK((qa[4] == Sample{350_ns + delays::kLs161Clear.fall, Zero}));
    }
    SECTION("CLK to RCO and ENT to RCO") {
        Bench b({{"CLK", Clock(16)},
                 {"CLR_N", ClearN()},
                 {"ENT", {One, {{1550_ns, Zero}, {1570_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1450_ns);
        sim.record(b.nets["RCO"]);
        sim.runUntil(1700_ns);
        const auto rco = sim.recorder().samples(b.nets["RCO"]);
        REQUIRE(rco.size() == 5);
        CHECK((rco[1] == Sample{1500_ns + delays::kLs161ClockToRco.rise, One}));
        CHECK((rco[2] == Sample{1550_ns + delays::kLs161EntToRco.fall, Zero}));
        CHECK((rco[3] == Sample{1570_ns + delays::kLs161EntToRco.rise, One}));
        CHECK((rco[4] == Sample{1600_ns + delays::kLs161ClockToRco.fall, Zero}));
    }
}
