#include "parts/gate_package.hpp"

#include <bit>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
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
};

constexpr GatePart kLs00{"74LS00", GateKind::Nand, 4, "AB", delays::kLs00};
constexpr GatePart kLs02{"74LS02", GateKind::Nor, 4, "AB", delays::kLs02};
constexpr GatePart kLs04{"74LS04", GateKind::Not, 6, "A", delays::kLs04};
constexpr GatePart kLs08{"74LS08", GateKind::And, 4, "AB", delays::kLs08};
constexpr GatePart kLs20{"74LS20", GateKind::Nand, 2, "ABCD", delays::kLs20};
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
TEST_CASE("gate package: 74LS08 truth table", "[parts]") { CheckTruthTable(kLs08); }
TEST_CASE("gate package: 74LS20 truth table", "[parts]") { CheckTruthTable(kLs20); }
TEST_CASE("gate package: 74LS30 truth table", "[parts]") { CheckTruthTable(kLs30); }
TEST_CASE("gate package: 74LS32 truth table", "[parts]") { CheckTruthTable(kLs32); }
TEST_CASE("gate package: 74LS86 truth table", "[parts]") { CheckTruthTable(kLs86); }

TEST_CASE("gate package: 74LS00 delays", "[parts]") { CheckDelays(kLs00); }
TEST_CASE("gate package: 74LS02 delays", "[parts]") { CheckDelays(kLs02); }
TEST_CASE("gate package: 74LS04 delays", "[parts]") { CheckDelays(kLs04); }
TEST_CASE("gate package: 74LS08 delays", "[parts]") { CheckDelays(kLs08); }
TEST_CASE("gate package: 74LS20 delays", "[parts]") { CheckDelays(kLs20); }
TEST_CASE("gate package: 74LS30 delays", "[parts]") { CheckDelays(kLs30); }
TEST_CASE("gate package: 74LS32 delays", "[parts]") { CheckDelays(kLs32); }
TEST_CASE("gate package: 74LS86 delays", "[parts]") { CheckDelays(kLs86); }
