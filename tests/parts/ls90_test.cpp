#include "parts/ls90.hpp"

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

constexpr const char* kOutputs[] = {"QA", "QB", "QC", "QD"};

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// Falling edges at 100 ns, 200 ns, ... (count of them), rising 50 ns before each.
Input Clock(int falls, Time start = 0) {
    Input clock{Zero, {}};
    for (int k = 1; k <= falls; ++k) {
        clock.steps.push_back({start + k * 100_ns - 50_ns, One});
        clock.steps.push_back({start + k * 100_ns, Zero});
    }
    return clock;
}

// Reset R01 = R02 high until 50 ns, then low.
Input ResetPulse() { return {One, {{50_ns, Zero}}}; }

// One 74LS90; listed inputs driven by Stimulus, the others float high (so leave R9 driven low).
// `linked`: QA wired to CKB, the decade wiring.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    Bench(std::map<std::string, Input> inputs, bool linked) {
        auto& chip = circuit.add<Ls90>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            for (std::string_view name : {"R01", "R91"}) {
                if (pin == name) {  // both pins of a pair on one Net
                    circuit.connect(chip, pin == "R01" ? "R02" : "R92", net);
                }
            }
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : kOutputs) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        if (linked) {
            circuit.connect(chip, "CKB", nets["QA"]);
        }
        circuit.finalize();
    }

    // Count on QD..QA, -1 if any bit is not 0/1.
    int count(const Simulator& sim) {
        int value = 0;
        for (int bit = 0; bit < 4; ++bit) {
            const LogicValue v = sim.value(nets[kOutputs[bit]]);
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << bit;
        }
        return value;
    }
};

// Settled time after the k-th falling edge of Clock().
Time After(int k, Time start = 0) { return start + k * 100_ns + 80_ns; }

}  // namespace

TEST_CASE("74LS90: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS90");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-14");
    const Ls90 chip("U1");
    const auto& pins = chip.pins();
    CHECK(pins[Ls90::kCkb].name == "CKB");
    CHECK(pins[Ls90::kCka].name == "CKA");
    CHECK(pins[Ls90::kCka].fallingEdgeOnly);  // acts only as it falls
    CHECK(pins[Ls90::kCkb].fallingEdgeOnly);
    CHECK_FALSE(pins[Ls90::kR01].fallingEdgeOnly);
    CHECK(pins[Ls90::kQA].name == "QA");
    CHECK(pins[Ls90::kQD].name == "QD");
    CHECK(pins[Ls90::kR01].name == "R01");
    CHECK(pins[Ls90::kR92].name == "R92");
    CHECK(pins[4].name == "VCC");
    CHECK(pins[4].kind == PinKind::Power);
    CHECK(pins[9].name == "GND");
    CHECK(pins[3].kind == PinKind::NoConnect);
    CHECK(pins[12].kind == PinKind::NoConnect);
}

TEST_CASE("74LS90: QA divides CKA by two on falling edges", "[parts]") {
    Bench b({{"CKA", Clock(3)}, {"CKB", {Zero, {}}}, {"R01", ResetPulse()}, {"R91", {Zero, {}}}},
            false);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(60_ns);
    CHECK(sim.value(b.nets["QA"]) == Zero);
    sim.runUntil(99_ns);  // CKA rose at 50 ns: nothing
    CHECK(sim.value(b.nets["QA"]) == Zero);
    sim.record(b.nets["QA"]);
    sim.runUntil(After(2));
    const auto qa = sim.recorder().samples(b.nets["QA"]);
    REQUIRE(qa.size() == 3);
    CHECK((qa[1] == Sample{100_ns + delays::kLs90CkaToQA.rise, One}));
    CHECK((qa[2] == Sample{200_ns + delays::kLs90CkaToQA.fall, Zero}));
}

TEST_CASE("74LS90: QB..QD divide CKB by five", "[parts]") {
    Bench b({{"CKA", {Zero, {}}}, {"CKB", Clock(6)}, {"R01", ResetPulse()}, {"R91", {Zero, {}}}},
            false);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    std::vector<int> fives;
    for (int k = 0; k <= 6; ++k) {
        sim.runUntil(k == 0 ? 60_ns : After(k));
        fives.push_back(b.count(sim) >> 1);
    }
    CHECK(fives == std::vector<int>{0, 1, 2, 3, 4, 0, 1});
}

TEST_CASE("74LS90: QA to CKB counts 0 to 9 and QD falls on 9 to 0", "[parts]") {
    Bench b({{"CKA", Clock(11)}, {"R01", ResetPulse()}, {"R91", {Zero, {}}}}, true);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    std::vector<int> counts;
    for (int k = 0; k <= 11; ++k) {
        sim.runUntil(k == 0 ? 60_ns : After(k));
        counts.push_back(b.count(sim));
        if (k == 9) {
            sim.record(b.nets["QD"]);
        }
    }
    CHECK(counts == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1});
    const auto qd = sim.recorder().samples(b.nets["QD"]);
    REQUIRE(qd.size() == 2);
    // CKA → QA, then QA → CKB → QD
    CHECK((qd[1] == Sample{1000_ns + delays::kLs90CkaToQA.fall + delays::kLs90CkbToQD.fall, Zero}));
}

TEST_CASE("74LS90: reset needs both R0 pins, set 9 wins", "[parts]") {
    SECTION("one R0 pin high does not clear") {
        Circuit circuit;
        auto& chip = circuit.add<Ls90>("U1");
        const NetId r01 = circuit.addNet("R01");
        const NetId r02 = circuit.addNet("R02");
        const NetId r9 = circuit.addNet("R9");
        auto& high = circuit.add<Stimulus>("S1", One, std::vector<Stimulus::Step>{});
        auto& low = circuit.add<Stimulus>("S2", Zero, std::vector<Stimulus::Step>{{50_ns, One}});
        auto& r9low = circuit.add<Stimulus>("S3", Zero, std::vector<Stimulus::Step>{});
        circuit.connect(high, Stimulus::kOut, r01);
        circuit.connect(low, Stimulus::kOut, r02);
        circuit.connect(r9low, Stimulus::kOut, r9);
        circuit.connect(chip, "R01", r01);
        circuit.connect(chip, "R02", r02);
        circuit.connect(chip, "R91", r9);
        circuit.connect(chip, "R92", r9);
        const NetId qa = circuit.addNet("QA");
        const NetId qd = circuit.addNet("QD");
        circuit.connect(chip, "QA", qa);
        circuit.connect(chip, "QD", qd);
        circuit.finalize();
        int nonZero = 0;
        for (std::uint64_t seed = 1; seed <= 8; ++seed) {
            Simulator sim(circuit);
            sim.powerOn(seed);
            sim.runUntil(40_ns);
            nonZero += sim.value(qa) == One || sim.value(qd) == One;
            sim.runUntil(100_ns);
            CHECK(sim.value(qa) == Zero);
            CHECK(sim.value(qd) == Zero);
        }
        CHECK(nonZero > 0);
    }
    SECTION("R9 sets 9 over R0") {
        Bench b({{"CKA", {Zero, {}}},
                 {"CKB", {Zero, {}}},
                 {"R01", {One, {}}},
                 {"R91", {Zero, {{100_ns, One}}}}},
                false);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(90_ns);
        CHECK(b.count(sim) == 0);
        sim.record(b.nets["QA"]);
        sim.runUntil(200_ns);
        CHECK(b.count(sim) == 9);
        const auto qa = sim.recorder().samples(b.nets["QA"]);
        REQUIRE(qa.size() == 2);
        CHECK((qa[1] == Sample{100_ns + delays::kLs90SetNine.rise, One}));
    }
}

TEST_CASE("74LS90: clear delay", "[parts]") {
    Bench b({{"CKA", Clock(1)},
             {"CKB", {Zero, {}}},
             {"R01", {Zero, {{300_ns, One}}}},
             {"R91", {Zero, {}}}},
            false);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(250_ns);
    const LogicValue before = sim.value(b.nets["QA"]);
    sim.record(b.nets["QA"]);
    sim.runUntil(400_ns);
    CHECK(sim.value(b.nets["QA"]) == Zero);
    if (before == One) {
        const auto qa = sim.recorder().samples(b.nets["QA"]);
        REQUIRE(qa.size() == 2);
        CHECK((qa[1] == Sample{300_ns + delays::kLs90Clear.fall, Zero}));
    }
}

TEST_CASE("74LS90: illegal five states recover", "[parts]") {
    // Seeds give every power-on ÷5 value; one CKB edge later, 5 and 6 read 2, 7 reads 0.
    std::set<int> seen;
    for (std::uint64_t seed = 1; seed <= 64; ++seed) {
        Bench b({{"CKA", {Zero, {}}}, {"CKB", Clock(1)}, {"R01", {Zero, {}}}, {"R91", {Zero, {}}}},
                false);
        Simulator sim(b.circuit);
        sim.powerOn(seed);
        sim.runUntil(40_ns);
        const int five = b.count(sim) >> 1;
        seen.insert(five);
        sim.runUntil(After(1));
        constexpr int kNext[] = {1, 2, 3, 4, 0, 2, 2, 0};
        CHECK((b.count(sim) >> 1) == kNext[five]);
    }
    CHECK(seen.size() == 8);
}

TEST_CASE("74LS90: X on R0 makes the count X until a clean clear", "[parts]") {
    Bench b({{"CKA", Clock(3)},
             {"CKB", {Zero, {}}},
             {"R01", {Zero, {{120_ns, X}, {160_ns, One}, {180_ns, Zero}}}},
             {"R91", {Zero, {}}}},
            false);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(150_ns);
    CHECK(b.count(sim) == -1);
    sim.runUntil(190_ns);
    CHECK(b.count(sim) == 0);
    sim.runUntil(After(2));
    CHECK(b.count(sim) == 1);
}

TEST_CASE("74LS90: an X clock edge counts nothing", "[parts]") {
    Bench b({{"CKA", {Zero, {{50_ns, One}, {100_ns, X}, {150_ns, Zero}}}},
             {"CKB", {Zero, {}}},
             {"R01", ResetPulse()},
             {"R91", {Zero, {}}}},
            false);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(300_ns);
    CHECK(b.count(sim) == 0);
}
