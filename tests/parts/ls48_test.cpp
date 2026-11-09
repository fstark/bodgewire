#include "parts/ls48.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "parts/ls47.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr const char* kSegments[] = {"SA", "SB", "SC", "SD", "SE", "SF", "SG"};

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One 74LS48; every input driven (defaults: 0 for A..D, 1 for the controls).
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls48>("U1");
        for (const char* pin : {"A", "B", "C", "D"}) {
            inputs.try_emplace(pin, Input{Zero, {}});
        }
        for (const char* pin : {"LT_N", "RBI_N", "BI_N"}) {
            inputs.try_emplace(pin, Input{One, {}});
        }
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : kSegments) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        circuit.finalize();
    }

    // Segments a..g: '1' lit (output high), '0' dark, 'x'.
    std::string lit(const Simulator& sim) {
        std::string text;
        for (const char* pin : kSegments) {
            const LogicValue v = sim.value(nets[pin]);
            text += v == One ? '1' : (v == Zero ? '0' : 'x');
        }
        return text;
    }
};

std::string Pattern(std::uint8_t segments) {
    std::string text;
    for (int k = 0; k < 7; ++k) {
        text += (segments >> k) & 1u ? '1' : '0';
    }
    return text;
}

Input Bit(unsigned value, int bit) { return {(value >> bit) & 1u ? One : Zero, {}}; }

}  // namespace

TEST_CASE("74LS48: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS48");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-16");
    const Ls48 chip("U1");
    const auto& pins = chip.pins();
    CHECK(pins[Ls48::kA].name == "A");
    CHECK(pins[Ls48::kBiN].name == "BI_N");
    CHECK(pins[Ls48::kSA].name == "SA");
    CHECK(pins[Ls48::kSG].name == "SG");
    CHECK(pins[7].name == "GND");
    CHECK(pins[15].name == "VCC");
    for (const char* name : kSegments) {
        CHECK(pins[chip.pinIndex(name)].kind == PinKind::Output);
    }
}

TEST_CASE("74LS48: segments high where the 74LS47 lights them", "[parts]") {
    for (unsigned v = 0; v < 16; ++v) {
        INFO(v);
        Bench b({{"A", Bit(v, 0)}, {"B", Bit(v, 1)}, {"C", Bit(v, 2)}, {"D", Bit(v, 3)}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(b.lit(sim) == Pattern(Ls47Segments(v, true, true, true)));
    }
}

TEST_CASE("74LS48: blanking, lamp test, ripple blanking", "[parts]") {
    SECTION("BI_N low blanks") {
        Bench b({{"A", {One, {}}}, {"BI_N", {Zero, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(b.lit(sim) == "0000000");
    }
    SECTION("LT_N low lights all") {
        Bench b({{"LT_N", {Zero, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(b.lit(sim) == "1111111");
    }
    SECTION("RBI_N low blanks a 0") {
        Bench b({{"RBI_N", {Zero, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(b.lit(sim) == "0000000");
    }
}

TEST_CASE("74LS48: delay and X", "[parts]") {
    SECTION("1 to 3 lights a after kLs48") {
        Bench b({{"A", {One, {}}}, {"B", {Zero, {{200_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.record(b.nets["SA"]);
        sim.runUntil(400_ns);
        CHECK(b.lit(sim) == Pattern(0x4F));
        const auto a = sim.recorder().samples(b.nets["SA"]);
        REQUIRE(!a.empty());
        CHECK((a.back() == Sample{200_ns + delays::kLs48.rise, One}));
    }
    SECTION("X on A makes only the disagreeing segments X") {
        Bench b({{"A", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(100_ns);
        CHECK(b.lit(sim) == "x11xxx0");
    }
}
