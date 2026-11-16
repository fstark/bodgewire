#include "parts/ls93.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
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

// Clear R01 = R02 high until 50 ns, then low.
Input ClearPulse() { return {One, {{50_ns, Zero}}}; }

enum class Link { None, QaToCkb, QdToCka };

// One 74LS93; listed inputs driven by Stimulus ("R01" drives both R0 pins).
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    Bench(std::map<std::string, Input> inputs, Link link) {
        auto& chip = circuit.add<Ls93>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            if (pin == "R01") {
                circuit.connect(chip, "R02", net);
            }
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : kOutputs) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        if (link == Link::QaToCkb) {
            circuit.connect(chip, "CKB", nets["QA"]);
        } else if (link == Link::QdToCka) {
            circuit.connect(chip, "CKA", nets["QD"]);
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

TEST_CASE("74LS93: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS93");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-14");
    const Ls93 chip("U1");
    const auto& pins = chip.pins();
    CHECK(pins[Ls93::kCkb].name == "CKB");
    CHECK(pins[Ls93::kCka].name == "CKA");
    CHECK(pins[Ls93::kCka].fallingEdgeOnly);  // acts only as it falls
    CHECK(pins[Ls93::kCkb].fallingEdgeOnly);
    CHECK_FALSE(pins[Ls93::kR01].fallingEdgeOnly);
    CHECK(pins[Ls93::kR01].name == "R01");
    CHECK(pins[Ls93::kR02].name == "R02");
    CHECK(pins[Ls93::kQA].name == "QA");
    CHECK(pins[Ls93::kQB].name == "QB");
    CHECK(pins[Ls93::kQC].name == "QC");
    CHECK(pins[Ls93::kQD].name == "QD");
    CHECK(pins[4].name == "VCC");
    CHECK(pins[4].kind == PinKind::Power);
    CHECK(pins[9].name == "GND");
    for (const std::size_t nc : {3u, 5u, 6u, 12u}) {
        CHECK(pins[nc].kind == PinKind::NoConnect);
    }
}

TEST_CASE("74LS93: QA divides CKA by two on falling edges", "[parts]") {
    Bench b({{"CKA", Clock(3)}, {"CKB", {Zero, {}}}, {"R01", ClearPulse()}}, Link::None);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(99_ns);  // CKA rose at 50 ns: nothing
    CHECK(sim.value(b.nets["QA"]) == Zero);
    sim.record(b.nets["QA"]);
    sim.runUntil(After(2));
    const auto qa = sim.recorder().samples(b.nets["QA"]);
    REQUIRE(qa.size() == 3);
    CHECK((qa[1] == Sample{100_ns + delays::kLs93CkaToQA.rise, One}));
    CHECK((qa[2] == Sample{200_ns + delays::kLs93CkaToQA.fall, Zero}));
}

TEST_CASE("74LS93: QB..QD divide CKB by eight", "[parts]") {
    Bench b({{"CKA", {Zero, {}}}, {"CKB", Clock(9)}, {"R01", ClearPulse()}}, Link::None);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    std::vector<int> eights;
    for (int k = 0; k <= 9; ++k) {
        sim.runUntil(k == 0 ? 60_ns : After(k));
        eights.push_back(b.count(sim) >> 1);
    }
    CHECK(eights == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 0, 1});
}

TEST_CASE("74LS93: QA to CKB counts 0 to 15", "[parts]") {
    Bench b({{"CKA", Clock(17)}, {"R01", ClearPulse()}}, Link::QaToCkb);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    std::vector<int> counts;
    for (int k = 0; k <= 17; ++k) {
        sim.runUntil(k == 0 ? 60_ns : After(k));
        counts.push_back(b.count(sim));
    }
    CHECK(counts == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1});
}

TEST_CASE("74LS93: the ÷8 ripples, QB first and QD last", "[parts]") {
    Bench b({{"CKA", {Zero, {}}}, {"CKB", Clock(8)}, {"R01", ClearPulse()}}, Link::None);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(After(7));  // 7
    for (const char* pin : {"QB", "QC", "QD"}) {
        sim.record(b.nets[pin]);
    }
    sim.runUntil(After(8));  // 7 → 0
    CHECK(b.count(sim) == 0);
    const auto qb = sim.recorder().samples(b.nets["QB"]);
    const auto qc = sim.recorder().samples(b.nets["QC"]);
    const auto qd = sim.recorder().samples(b.nets["QD"]);
    REQUIRE(qb.size() == 2);
    REQUIRE(qc.size() == 2);
    REQUIRE(qd.size() == 2);
    CHECK((qb[1] == Sample{800_ns + delays::kLs93CkbToQB.fall, Zero}));
    CHECK((qc[1] == Sample{800_ns + delays::kLs93CkbToQC.fall, Zero}));
    CHECK((qd[1] == Sample{800_ns + delays::kLs93CkbToQD.fall, Zero}));
    CHECK(qb[1].time < qc[1].time);
    CHECK(qc[1].time < qd[1].time);
}

TEST_CASE("74LS93: clear needs both R0 pins and ignores clocks", "[parts]") {
    SECTION("one R0 pin high does not clear") {
        Circuit circuit;
        auto& chip = circuit.add<Ls93>("U1");
        const NetId r01 = circuit.addNet("R01");
        const NetId r02 = circuit.addNet("R02");
        auto& high = circuit.add<Stimulus>("S1", One, std::vector<Stimulus::Step>{});
        auto& low = circuit.add<Stimulus>("S2", Zero, std::vector<Stimulus::Step>{{50_ns, One}});
        circuit.connect(high, Stimulus::kOut, r01);
        circuit.connect(low, Stimulus::kOut, r02);
        circuit.connect(chip, "R01", r01);
        circuit.connect(chip, "R02", r02);
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
    SECTION("a CKB fall during the clear counts nothing") {
        Bench b({{"CKA", {Zero, {}}}, {"CKB", Clock(2)}, {"R01", {One, {{250_ns, Zero}}}}},
                Link::None);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(After(2));
        CHECK(b.count(sim) == 0);
    }
}

TEST_CASE("74LS93: clear delay", "[parts]") {
    Bench b({{"CKA", Clock(1)}, {"CKB", {Zero, {}}}, {"R01", {Zero, {{300_ns, One}}}}}, Link::None);
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
        CHECK((qa[1] == Sample{300_ns + delays::kLs93Clear.fall, Zero}));
    }
}

TEST_CASE("74LS93: QD to CKA sets QA on the 8th CKB fall", "[parts]") {
    // The bouncing ball's height counter: QA stays low for 8 CKB falls after a clear.
    Bench b({{"CKB", Clock(9)}, {"R01", ClearPulse()}}, Link::QdToCka);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    for (int k = 1; k <= 7; ++k) {
        sim.runUntil(After(k));
        CHECK(sim.value(b.nets["QA"]) == Zero);
    }
    sim.runUntil(After(8));
    CHECK(sim.value(b.nets["QA"]) == One);
    CHECK((b.count(sim) >> 1) == 0);
}

TEST_CASE("74LS93: X on R0 makes the count X until a clean clear", "[parts]") {
    Bench b({{"CKA", Clock(3)},
             {"CKB", {Zero, {}}},
             {"R01", {Zero, {{120_ns, X}, {160_ns, One}, {180_ns, Zero}}}}},
            Link::None);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(150_ns);
    CHECK(b.count(sim) == -1);
    sim.runUntil(190_ns);
    CHECK(b.count(sim) == 0);
    sim.runUntil(After(2));
    CHECK(b.count(sim) == 1);
}

TEST_CASE("74LS93: an X clock edge counts nothing", "[parts]") {
    Bench b({{"CKA", {Zero, {{50_ns, One}, {100_ns, X}, {150_ns, Zero}}}},
             {"CKB", {Zero, {}}},
             {"R01", ClearPulse()}},
            Link::None);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(300_ns);
    CHECK(b.count(sim) == 0);
}
