#include "parts/ls47.hpp"

#include <map>
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

constexpr const char* kSegments[] = {"SA_N", "SB_N", "SC_N", "SD_N", "SE_N", "SF_N", "SG_N"};

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One 74LS47; every input driven (defaults: 0 for A..D, 1 for the controls), each output Net
// pulled up weakly so a released output reads 1.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs) {
        auto& chip = circuit.add<Ls47>("U1");
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
            circuit.tie(nets[pin], One, DriveStrength::Weak);
        }
        circuit.finalize();
    }

    // Segments a..g: '1' lit (output low), '0' dark, 'x'.
    std::string lit(const Simulator& sim) {
        std::string text;
        for (const char* pin : kSegments) {
            const LogicValue v = sim.value(nets[pin]);
            text += v == Zero ? '1' : (v == One ? '0' : 'x');
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

}  // namespace

TEST_CASE("74LS47: catalogue entry", "[parts]") {
    const PartInfo* part = FindPart("74LS47");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Chip);
    CHECK(part->package == "DIP-16");
    const Ls47 chip("U1");
    const auto& pins = chip.pins();
    CHECK(pins[Ls47::kA].name == "A");
    CHECK(pins[Ls47::kD].name == "D");
    CHECK(pins[Ls47::kBiN].name == "BI_N");
    CHECK(pins[Ls47::kSA].name == "SA_N");
    CHECK(pins[Ls47::kSG].name == "SG_N");
    for (const char* name : kSegments) {
        CHECK(pins[chip.pinIndex(name)].kind == PinKind::OpenCollector);
    }
}

TEST_CASE("74LS47: glyph table", "[parts]") {
    const std::vector<std::uint8_t> expected{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07,
                                             0x7F, 0x67, 0x58, 0x4C, 0x62, 0x69, 0x78, 0x00};
    for (unsigned v = 0; v < 16; ++v) {
        INFO(v);
        CHECK(Ls47Segments(v, true, true, true) == expected[v]);
    }
    CHECK(Ls47Segments(6, true, true, true) == 0x7C);  // no a: tail-less 6
    CHECK(Ls47Segments(9, true, true, true) == 0x67);  // no d: tail-less 9
}

TEST_CASE("74LS47: lamp test, blanking, ripple blanking", "[parts]") {
    CHECK(Ls47Segments(1, false, true, true) == 0x7F);
    CHECK(Ls47Segments(1, false, true, false) == 0x00);  // BI_N wins
    CHECK(Ls47Segments(5, true, true, false) == 0x00);
    CHECK(Ls47Segments(0, true, false, true) == 0x00);
    CHECK(Ls47Segments(3, true, false, true) == 0x4F);   // RBI_N blanks only a 0
    CHECK(Ls47Segments(0, false, false, true) == 0x7F);  // lamp test over ripple blanking
}

TEST_CASE("74LS47: outputs sink lit segments and release the others", "[parts]") {
    Bench b({{"A", {One, {}}}, {"B", {Zero, {{200_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(b.lit(sim) == Pattern(0x06));  // 1
    CHECK(!sim.isFloating(b.nets["SA_N"]));
    sim.record(b.nets["SA_N"]);
    sim.runUntil(400_ns);
    CHECK(b.lit(sim) == Pattern(0x4F));  // 3
    const auto a = sim.recorder().samples(b.nets["SA_N"]);
    REQUIRE(a.size() == 2);
    CHECK((a[1] == Sample{200_ns + delays::kLs47.fall, Zero}));
}

TEST_CASE("74LS47: X on an input makes only the disagreeing segments X", "[parts]") {
    Bench b({{"A", {X, {}}}});  // 0 or 1
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    // a b c d e f g: 0 lights a-f, 1 lights b c.
    CHECK(b.lit(sim) == "x11xxx0");
}

TEST_CASE("74LS47: X on BI_N", "[parts]") {
    Bench b({{"BI_N", {X, {}}}, {"A", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(b.lit(sim) == "0xx0000");
}
