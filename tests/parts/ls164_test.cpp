#include "parts/ls164.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr const char* kOutputs[] = {"QA", "QB", "QC", "QD", "QE", "QF", "QG", "QH"};

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

// One 74LS164; listed inputs driven by Stimulus, the others float high.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls164>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : kOutputs) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }

    // QA..QH as '0' / '1' / 'x'.
    std::string stages(const Simulator& sim) {
        std::string text;
        for (const char* pin : kOutputs) {
            text += ToChar(sim.value(nets[pin]));
        }
        return text;
    }
};

// Settled sample time after the k-th rising edge of Clock().
Time After(int k) { return k * 100_ns + 60_ns; }

}  // namespace

TEST_CASE("74LS164: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS164");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-14");
    const Ls164 chip("U1");
    const auto& pins = chip.pins();
    CHECK(pins[Ls164::kA].name == "A");
    CHECK(pins[Ls164::kB].name == "B");
    CHECK(pins[Ls164::kQA].name == "QA");
    CHECK(pins[Ls164::kQD].name == "QD");
    CHECK(pins[Ls164::kClk].name == "CLK");
    CHECK(pins[Ls164::kClrN].name == "CLR_N");
    CHECK(pins[Ls164::kQE].name == "QE");
    CHECK(pins[Ls164::kQH].name == "QH");
}

TEST_CASE("74LS164: a one walks to QH", "[parts]") {
    Bench b(
        {{"CLK", Clock(8)}, {"CLR_N", ClearN()}, {"A", {One, {{150_ns, Zero}}}}, {"B", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(60_ns);
    CHECK(b.stages(sim) == "00000000");
    sim.record(b.nets["QA"]);
    sim.record(b.nets["QH"]);
    sim.runUntil(After(8));
    const auto qa = sim.recorder().samples(b.nets["QA"]);
    REQUIRE(qa.size() == 3);
    CHECK((qa[1] == Sample{100_ns + delays::kLs164ClockToQ.rise, One}));
    CHECK((qa[2] == Sample{200_ns + delays::kLs164ClockToQ.fall, Zero}));
    const auto qh = sim.recorder().samples(b.nets["QH"]);
    REQUIRE(qh.size() == 2);
    CHECK((qh[1] == Sample{800_ns + delays::kLs164ClockToQ.rise, One}));
    CHECK(b.stages(sim) == "00000001");
}

TEST_CASE("74LS164: A and B are ANDed", "[parts]") {
    const std::vector<std::pair<std::pair<LogicValue, LogicValue>, LogicValue>> cases{
        {{One, Zero}, Zero}, {{Zero, One}, Zero}, {{X, Zero}, Zero}, {{One, One}, One}};
    for (const auto& [inputs, expected] : cases) {
        Bench b({{"CLK", Clock(1)},
                 {"CLR_N", ClearN()},
                 {"A", {inputs.first, {}}},
                 {"B", {inputs.second, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(After(1));
        CHECK(sim.value(b.nets["QA"]) == expected);
    }
}

TEST_CASE("74LS164: clear", "[parts]") {
    Bench b({{"CLK", Clock(10)},
             {"CLR_N", ClearN({{850_ns, Zero}})},
             {"A", {One, {}}},
             {"B", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(8));
    CHECK(b.stages(sim) == "11111111");
    sim.record(b.nets["QH"]);
    sim.runUntil(After(10));
    const auto qh = sim.recorder().samples(b.nets["QH"]);
    REQUIRE(qh.size() == 2);
    CHECK((qh[1] == Sample{850_ns + delays::kLs164Clear.fall, Zero}));
    CHECK(b.stages(sim) == "00000000");
}

TEST_CASE("74LS164: data changing with the edge is not seen", "[parts]") {
    Bench b(
        {{"CLK", Clock(2)}, {"CLR_N", ClearN()}, {"A", {Zero, {{100_ns, One}}}}, {"B", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(1));
    CHECK(sim.value(b.nets["QA"]) == Zero);
    sim.runUntil(After(2));
    CHECK(sim.value(b.nets["QA"]) == One);
}

TEST_CASE("74LS164: power-on outputs are known", "[parts]") {
    std::set<std::string> patterns;
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        Bench b({});
        Simulator sim(b.circuit);
        sim.powerOn(seed);
        sim.runUntil(10_ns);
        const std::string stages = b.stages(sim);
        CHECK(stages.find('x') == std::string::npos);
        patterns.insert(stages);
    }
    CHECK(patterns.size() > 1);
}
