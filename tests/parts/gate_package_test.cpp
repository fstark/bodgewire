#include "parts/gate_package.hpp"

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
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

// Expected behaviour from the datasheet, independent of the GatePackageSpec tables.
struct GatePart {
    std::string_view name;
    GateKind kind;
    int units;
    std::string_view inputs;  // input pin letters; pins are "<unit><letter>", output "<unit>Y"
    Delays delays;
    bool openCollector = false;  // outputs pulled up on the bench
};

constexpr GatePart kLs00{"74LS00", GateKind::Nand, 4, "AB", delays::kLs00};
constexpr GatePart kLs02{"74LS02", GateKind::Nor, 4, "AB", delays::kLs02};
constexpr GatePart kLs04{"74LS04", GateKind::Not, 6, "A", delays::kLs04};
constexpr GatePart kLs05{"74LS05", GateKind::Not, 6, "A", delays::kLs05, true};
constexpr GatePart kLs08{"74LS08", GateKind::And, 4, "AB", delays::kLs08};
constexpr GatePart kLs10{"74LS10", GateKind::Nand, 3, "ABC", delays::kLs10};
constexpr GatePart kLs20{"74LS20", GateKind::Nand, 2, "ABCD", delays::kLs20};
constexpr GatePart kLs21{"74LS21", GateKind::And, 2, "ABCD", delays::kLs21};
constexpr GatePart kTtl17{"7417", GateKind::Buffer, 6, "A", delays::kTtl17, true};
constexpr GatePart kLs27{"74LS27", GateKind::Nor, 3, "ABC", delays::kLs27};
constexpr GatePart kLs30{"74LS30", GateKind::Nand, 1, "ABCDEFGH", delays::kLs30};
constexpr GatePart kLs32{"74LS32", GateKind::Or, 4, "AB", delays::kLs32};
constexpr GatePart kLs86{"74LS86", GateKind::Xor, 4, "AB", delays::kLs86};

LogicValue FromBool(bool b) { return b ? One : Zero; }

bool Bit(unsigned bits, std::size_t i) { return ((bits >> i) & 1u) != 0; }

LogicValue Expected(GateKind kind, unsigned bits, std::size_t inputCount) {
    const bool all = bits == (1u << inputCount) - 1;
    const bool any = bits != 0;
    switch (kind) {
    case GateKind::And:
        return FromBool(all);
    case GateKind::Nand:
        return FromBool(!all);
    case GateKind::Or:
    case GateKind::Buffer:
        return FromBool(any);
    case GateKind::Nor:
    case GateKind::Not:
        return FromBool(!any);
    case GateKind::Xor:
        return FromBool(std::popcount(bits) % 2 == 1);
    default:
        return X;
    }
}

std::string PinName(const GatePart& part, int unit, std::string_view suffix) {
    return (part.units > 1 ? std::to_string(unit + 1) : std::string()) + std::string(suffix);
}

std::vector<unsigned> Combinations(std::size_t inputCount) {
    std::vector<unsigned> combos;
    const unsigned allOnes = (1u << inputCount) - 1;
    if (inputCount <= 4) {
        for (unsigned bits = 0; bits <= allOnes; ++bits) {
            combos.push_back(bits);
        }
        return combos;
    }
    combos.push_back(allOnes);
    for (std::size_t i = 0; i < inputCount; ++i) {
        combos.push_back(allOnes & ~(1u << i));
    }
    return combos;
}

// Every unit gets the same input sequence, one combination every 100 ns.
void CheckTruthTable(const GatePart& part) {
    const std::size_t n = part.inputs.size();
    const std::vector<unsigned> combos = Combinations(n);
    Circuit circuit;
    Component& chip = *FindPart(part.name)->create(circuit, "U1", {}).value();
    std::vector<NetId> outputs;
    for (int unit = 0; unit < part.units; ++unit) {
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<Stimulus::Step> steps;
            for (std::size_t k = 1; k < combos.size(); ++k) {
                steps.push_back({static_cast<Time>(k) * 100_ns, FromBool(Bit(combos[k], i))});
            }
            const std::string pin = PinName(part, unit, part.inputs.substr(i, 1));
            const NetId net = circuit.addNet(pin);
            auto& source =
                circuit.add<Stimulus>("S" + pin, FromBool(Bit(combos[0], i)), std::move(steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
        }
        const std::string pin = PinName(part, unit, "Y");
        outputs.push_back(circuit.addNet(pin));
        if (part.openCollector) {
            circuit.tie(outputs.back(), One, DriveStrength::Weak);
        }
        circuit.connect(chip, pin, outputs.back());
    }
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    for (std::size_t k = 0; k < combos.size(); ++k) {
        sim.runUntil(static_cast<Time>(k) * 100_ns + 50_ns);
        for (int unit = 0; unit < part.units; ++unit) {
            INFO("unit " << unit + 1 << " inputs " << combos[k]);
            CHECK(sim.value(outputs[unit]) == Expected(part.kind, combos[k], n));
        }
    }
}

// Unit 1: first input 0 → 1 at 100 ns, 1 → 0 at 200 ns, others non-controlling.
void CheckDelays(const GatePart& part) {
    const bool othersHigh = part.kind == GateKind::And || part.kind == GateKind::Nand;
    Circuit circuit;
    Component& chip = *FindPart(part.name)->create(circuit, "U1", {}).value();
    for (std::size_t i = 0; i < part.inputs.size(); ++i) {
        const std::string pin = PinName(part, 0, part.inputs.substr(i, 1));
        const NetId net = circuit.addNet(pin);
        std::vector<Stimulus::Step> steps;
        LogicValue initial = FromBool(othersHigh);
        if (i == 0) {
            initial = Zero;
            steps = {{100_ns, One}, {200_ns, Zero}};
        }
        auto& source = circuit.add<Stimulus>("S" + pin, initial, std::move(steps));
        circuit.connect(source, Stimulus::kOut, net);
        circuit.connect(chip, pin, net);
    }
    const NetId y = circuit.addNet("Y");
    if (part.openCollector) {
        circuit.tie(y, One, DriveStrength::Weak);
    }
    circuit.connect(chip, PinName(part, 0, "Y"), y);
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    sim.record(y);
    sim.runUntil(300_ns);
    const auto samples = sim.recorder().samples(y);
    REQUIRE(samples.size() == 3);
    CHECK(samples[1].value != samples[2].value);
    for (std::size_t s = 1; s < 3; ++s) {
        const Time inputChange = static_cast<Time>(s) * 100_ns;
        const Time expected = samples[s].value == One ? part.delays.rise : part.delays.fall;
        CHECK(samples[s].time - inputChange == expected);
    }
}

}  // namespace

TEST_CASE("gate package: 74LS00 truth table", "[parts]") { CheckTruthTable(kLs00); }
TEST_CASE("gate package: 74LS02 truth table", "[parts]") { CheckTruthTable(kLs02); }
TEST_CASE("gate package: 74LS04 truth table", "[parts]") { CheckTruthTable(kLs04); }
TEST_CASE("gate package: 74LS05 truth table", "[parts]") { CheckTruthTable(kLs05); }
TEST_CASE("gate package: 74LS08 truth table", "[parts]") { CheckTruthTable(kLs08); }
TEST_CASE("gate package: 74LS10 truth table", "[parts]") { CheckTruthTable(kLs10); }
TEST_CASE("gate package: 74LS20 truth table", "[parts]") { CheckTruthTable(kLs20); }
TEST_CASE("gate package: 74LS21 truth table", "[parts]") { CheckTruthTable(kLs21); }
TEST_CASE("gate package: 74LS27 truth table", "[parts]") { CheckTruthTable(kLs27); }
TEST_CASE("gate package: 7417 truth table", "[parts]") { CheckTruthTable(kTtl17); }
TEST_CASE("gate package: 74LS30 truth table", "[parts]") { CheckTruthTable(kLs30); }
TEST_CASE("gate package: 74LS32 truth table", "[parts]") { CheckTruthTable(kLs32); }
TEST_CASE("gate package: 74LS86 truth table", "[parts]") { CheckTruthTable(kLs86); }

TEST_CASE("gate package: 74LS00 delays", "[parts]") { CheckDelays(kLs00); }
TEST_CASE("gate package: 74LS02 delays", "[parts]") { CheckDelays(kLs02); }
TEST_CASE("gate package: 74LS04 delays", "[parts]") { CheckDelays(kLs04); }
TEST_CASE("gate package: 74LS05 delays", "[parts]") { CheckDelays(kLs05); }
TEST_CASE("gate package: 74LS08 delays", "[parts]") { CheckDelays(kLs08); }
TEST_CASE("gate package: 74LS10 delays", "[parts]") { CheckDelays(kLs10); }
TEST_CASE("gate package: 74LS20 delays", "[parts]") { CheckDelays(kLs20); }
TEST_CASE("gate package: 74LS21 delays", "[parts]") { CheckDelays(kLs21); }
TEST_CASE("gate package: 74LS27 delays", "[parts]") { CheckDelays(kLs27); }
TEST_CASE("gate package: 7417 delays", "[parts]") { CheckDelays(kTtl17); }

TEST_CASE("gate package: 7417 releases its 1", "[parts]") {
    // Open collector: a high input lets the output float; only a pull-up makes it 1.
    Circuit circuit;
    Component& chip = *FindPart("7417")->create(circuit, "U1", {}).value();
    CHECK(chip.pins()[1].kind == PinKind::OpenCollector);  // 1Y
    CHECK(chip.pins()[0].kind == PinKind::Input);          // 1A
    const NetId a = circuit.addNet("1A");
    auto& source = circuit.add<Stimulus>("SA", One, std::vector<Stimulus::Step>{{100_ns, Zero}});
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(chip, "1A", a);
    const NetId y = circuit.addNet("1Y");
    circuit.connect(chip, "1Y", y);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.isFloating(y));
    sim.runUntil(200_ns);
    CHECK(sim.value(y) == Zero);
}
TEST_CASE("gate package: 74LS05 releases its 1", "[parts]") {
    // Open collector: a low input lets the output float; only a pull-up makes it 1.
    Circuit circuit;
    Component& chip = *FindPart("74LS05")->create(circuit, "U1", {}).value();
    CHECK(chip.pins()[1].kind == PinKind::OpenCollector);  // 1Y
    const NetId a = circuit.addNet("1A");
    auto& source = circuit.add<Stimulus>("SA", Zero, std::vector<Stimulus::Step>{{100_ns, One}});
    circuit.connect(source, Stimulus::kOut, a);
    circuit.connect(chip, "1A", a);
    const NetId y = circuit.addNet("1Y");
    circuit.connect(chip, "1Y", y);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.isFloating(y));
    sim.runUntil(200_ns);
    CHECK(sim.value(y) == Zero);
}
TEST_CASE("gate package: 74LS30 delays", "[parts]") { CheckDelays(kLs30); }
TEST_CASE("gate package: 74LS32 delays", "[parts]") { CheckDelays(kLs32); }
TEST_CASE("gate package: 74LS86 delays", "[parts]") { CheckDelays(kLs86); }

TEST_CASE("gate package: 74LS27 pinout", "[parts]") {
    Circuit circuit;
    const Component& chip = *FindPart("74LS27")->create(circuit, "U1", {}).value();
    const std::vector<std::string_view> expected{"1A", "1B", "2A", "2B", "2C", "2Y", "GND",
                                                 "3Y", "3A", "3B", "3C", "1Y", "1C", "VCC"};
    REQUIRE(chip.pins().size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(chip.pins()[i].name == expected[i]);
    }
    CHECK(chip.pins()[11].kind == PinKind::Output);  // 1Y on pin 12
    CHECK(chip.pins()[12].kind == PinKind::Input);   // 1C on pin 13
}

TEST_CASE("gate package: X input propagates", "[parts]") {
    Circuit circuit;
    Component& chip = *FindPart("74LS00")->create(circuit, "U1", {}).value();
    const NetId a = circuit.addNet("1A");
    const NetId b = circuit.addNet("1B");
    const NetId y = circuit.addNet("1Y");
    auto& sourceA = circuit.add<Stimulus>("SA", X, std::vector<Stimulus::Step>{});
    auto& sourceB = circuit.add<Stimulus>("SB", One, std::vector<Stimulus::Step>{{100_ns, Zero}});
    circuit.connect(sourceA, Stimulus::kOut, a);
    circuit.connect(sourceB, Stimulus::kOut, b);
    circuit.connect(chip, "1A", a);
    circuit.connect(chip, "1B", b);
    circuit.connect(chip, "1Y", y);
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    CHECK(sim.value(y) == X);
    sim.runUntil(150_ns);
    CHECK(sim.value(y) == One);
}

TEST_CASE("gate package: the power-on evaluation drives every unit, nothing having changed",
          "[parts]") {
    // 74LS04, every input held at 1 from power-on: every output 0 whatever it started at.
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        Circuit circuit;
        Component& chip = *FindPart("74LS04")->create(circuit, "U1", {}).value();
        std::vector<NetId> outputs;
        for (int unit = 1; unit <= 6; ++unit) {
            const std::string a = std::to_string(unit) + "A";
            const NetId in = circuit.addNet(a);
            circuit.connect(circuit.add<Stimulus>("S" + a, One, std::vector<Stimulus::Step>{}),
                            Stimulus::kOut, in);
            circuit.connect(chip, a, in);
            outputs.push_back(circuit.addNet(std::to_string(unit) + "Y"));
            circuit.connect(chip, std::to_string(unit) + "Y", outputs.back());
        }
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(seed);
        sim.runUntil(100_ns);
        for (const NetId y : outputs) {
            INFO("seed " << seed);
            CHECK(sim.value(y) == Zero);
        }
    }
}

TEST_CASE("gate package: a unit's pending change survives another unit's input change", "[parts]") {
    // A regression guard: 74LS04 unit 1 input rises at 100 ns (its output falls kLs04.fall
    // later); unit 2's input moves at 101 ns, waking the chip before unit 1's output changes.
    Circuit circuit;
    Component& chip = *FindPart("74LS04")->create(circuit, "U1", {}).value();
    const NetId a1 = circuit.addNet("1A");
    const NetId a2 = circuit.addNet("2A");
    const NetId y1 = circuit.addNet("1Y");
    circuit.connect(circuit.add<Stimulus>("S1", Zero, std::vector<Stimulus::Step>{{100_ns, One}}),
                    Stimulus::kOut, a1);
    circuit.connect(circuit.add<Stimulus>("S2", Zero, std::vector<Stimulus::Step>{{101_ns, One}}),
                    Stimulus::kOut, a2);
    circuit.connect(chip, "1A", a1);
    circuit.connect(chip, "2A", a2);
    circuit.connect(chip, "1Y", y1);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + delays::kLs04.fall - 1);
    CHECK(sim.value(y1) == One);
    sim.runUntil(100_ns + delays::kLs04.fall);
    CHECK(sim.value(y1) == Zero);
}
