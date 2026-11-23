#include "parts/ls107.hpp"

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

// Falling edges at 100 ns, 200 ns, ... (count of them), rising 50 ns before each.
Input Clock(int falls) {
    Input clock{Zero, {}};
    for (int k = 1; k <= falls; ++k) {
        clock.steps.push_back({k * 100_ns - 50_ns, One});
        clock.steps.push_back({k * 100_ns, Zero});
    }
    return clock;
}

// Clear low until 30 ns, then high.
Input ClearPulse() { return {Zero, {{30_ns, One}}}; }

// One 74LS107; listed inputs driven by Stimulus, the others float high. Outputs on nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls107>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
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

// Flip-flop 1 or 2, Q cleared until 30 ns, then the given J, K and clock.
Bench Unit(int unit, Input j, Input k, Input clock) {
    const std::string u = std::to_string(unit);
    return Bench({{u + "J", std::move(j)},
                  {u + "K", std::move(k)},
                  {u + "CLK", std::move(clock)},
                  {u + "CLR_N", ClearPulse()}});
}

// Q after each falling edge of Clock().
std::vector<LogicValue> Qs(Bench& b, int unit, int falls) {
    Simulator sim(b.circuit);
    sim.powerOn(1);
    std::vector<LogicValue> qs;
    for (int k = 0; k <= falls; ++k) {
        sim.runUntil(k * 100_ns + 60_ns);
        qs.push_back(sim.value(b.nets[std::to_string(unit) + "Q"]));
    }
    return qs;
}

}  // namespace

TEST_CASE("74LS107: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS107");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-14");
    const Ls107 chip("U1");
    const std::vector<std::string> expected{"1J", "1Q_N", "1Q",     "1K", "2Q",   "2Q_N",   "GND",
                                            "2J", "2CLK", "2CLR_N", "2K", "1CLK", "1CLR_N", "VCC"};
    REQUIRE(chip.pins().size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(chip.pins()[i].name == expected[i]);
    }
    CHECK(chip.pins()[2].kind == PinKind::Output);
    CHECK(chip.pins()[11].kind == PinKind::Input);
    for (const std::size_t clk : {8u, 11u}) {  // 2CLK, 1CLK: act on the falling edge only
        CHECK_FALSE(chip.pins()[clk].risingEdgeOnly);
        CHECK(chip.pins()[clk].fallingEdgeOnly);
    }
    for (const std::size_t jk : {0u, 3u, 7u, 10u}) {  // 1J, 1K, 2J, 2K: read at the edge only
        CHECK(chip.pins()[jk].sampled);
    }
    CHECK_FALSE(chip.pins()[12].sampled);  // 1CLR_N acts by itself
    CHECK_FALSE(chip.pins()[12].fallingEdgeOnly);
}

TEST_CASE("74LS107: set, clear, hold and toggle on the falling edge", "[parts]") {
    const int unit = GENERATE(1, 2);
    INFO("flip-flop " << unit);
    SECTION("J high sets, K high clears, both low hold") {
        // J 1 0 0 0, K 0 0 1 0 at the falls at 100, 200, 300, 400 ns.
        Bench b =
            Unit(unit, {One, {{150_ns, Zero}}}, {Zero, {{250_ns, One}, {350_ns, Zero}}}, Clock(4));
        CHECK(Qs(b, unit, 4) == std::vector<LogicValue>{Zero, One, One, Zero, Zero});
    }
    SECTION("both high toggle") {
        Bench b = Unit(unit, {One, {}}, {One, {}}, Clock(4));
        CHECK(Qs(b, unit, 4) == std::vector<LogicValue>{Zero, One, Zero, One, Zero});
    }
}

TEST_CASE("74LS107: a rising edge changes nothing", "[parts]") {
    Bench b = Unit(1, {One, {}}, {Zero, {}}, {Zero, {{50_ns, One}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(200_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
}

TEST_CASE("74LS107: J and K are read before the edge", "[parts]") {
    // J rises at the very timestamp of the fall: not seen.
    Bench b = Unit(1, {Zero, {{100_ns, One}}}, {Zero, {}}, Clock(1));
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(160_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
}

TEST_CASE("74LS107: clear wins over the clock, at once", "[parts]") {
    Bench b({{"1J", {One, {}}},
             {"1K", {Zero, {}}},
             {"1CLK", Clock(3)},
             {"1CLR_N", {Zero, {{30_ns, One}, {250_ns, Zero}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(160_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    sim.record(b.nets["1Q"]);
    sim.record(b.nets["1Q_N"]);
    sim.runUntil(400_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);  // the fall at 300 ns does not set it
    const auto q = sim.recorder().samples(b.nets["1Q"]);
    const auto qn = sim.recorder().samples(b.nets["1Q_N"]);
    REQUIRE(q.size() == 2);
    REQUIRE(qn.size() == 2);
    CHECK((q[1] == Sample{250_ns + delays::kLs107.fall, Zero}));
    CHECK((qn[1] == Sample{250_ns + delays::kLs107.rise, One}));
}

TEST_CASE("74LS107: clock delay", "[parts]") {
    Bench b = Unit(1, {One, {}}, {One, {}}, Clock(2));
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(60_ns);
    sim.record(b.nets["1Q"]);
    sim.runUntil(260_ns);
    const auto q = sim.recorder().samples(b.nets["1Q"]);
    REQUIRE(q.size() == 3);
    CHECK((q[1] == Sample{100_ns + delays::kLs107.rise, One}));
    CHECK((q[2] == Sample{200_ns + delays::kLs107.fall, Zero}));
}

TEST_CASE("74LS107: X on J", "[parts]") {
    SECTION("with K low and Q high: hold or set, stays 1") {
        // Set at 100 ns, then J at X.
        Bench b = Unit(1, {One, {{150_ns, X}}}, {Zero, {}}, Clock(2));
        CHECK(Qs(b, 1, 2) == std::vector<LogicValue>{Zero, One, One});
    }
    SECTION("with K high and Q low: clear or toggle, X") {
        Bench b = Unit(1, {X, {}}, {One, {}}, Clock(1));
        CHECK(Qs(b, 1, 1) == std::vector<LogicValue>{Zero, X});
    }
}

TEST_CASE("74LS107: X on clear, then a clean clear recovers", "[parts]") {
    Bench b({{"1J", {Zero, {}}},
             {"1K", {Zero, {}}},
             {"1CLK", {Zero, {}}},
             {"1CLR_N", {One, {{50_ns, X}, {100_ns, Zero}, {150_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["1Q"]) == X);
    sim.runUntil(200_ns);
    CHECK(sim.value(b.nets["1Q"]) == Zero);
    CHECK(sim.value(b.nets["1Q_N"]) == One);
}

TEST_CASE("74LS107: the two flip-flops are independent", "[parts]") {
    Bench b({{"1J", {One, {}}},
             {"1K", {One, {}}},
             {"1CLK", Clock(3)},
             {"1CLR_N", ClearPulse()},
             {"2J", {One, {}}},
             {"2K", {One, {}}},
             {"2CLK", {Zero, {}}},
             {"2CLR_N", ClearPulse()}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(360_ns);
    CHECK(sim.value(b.nets["1Q"]) == One);
    CHECK(sim.value(b.nets["2Q"]) == Zero);
}
