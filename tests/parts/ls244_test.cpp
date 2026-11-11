#include "parts/ls244.hpp"

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using namespace delays;

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One 74LS244; every input driven by a Stimulus (listed, or 0 for an A, 1 for an OE_N), every
// output on a Net of its own.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(const std::map<std::string, Input>& inputs) {
        auto& chip = circuit.add<Ls244>("U1");
        const auto pins = chip.pins();
        for (PinIndex pin = 0; pin < pins.size(); ++pin) {
            const std::string name = pins[pin].name;
            if (pins[pin].kind == PinKind::Power) {
                continue;
            }
            const NetId net = circuit.addNet(name);
            nets[name] = net;
            circuit.connect(chip, pin, net);
            if (pins[pin].kind != PinKind::Input) {
                continue;
            }
            Input input{name.ends_with("OE_N") ? One : Zero, {}};
            if (const auto it = inputs.find(name); it != inputs.end()) {
                input = it->second;
            }
            auto& source = circuit.add<Stimulus>("S" + name, input.initial, input.steps);
            circuit.connect(source, Stimulus::kOut, net);
        }
        circuit.finalize();
    }

    LogicValue value(const Simulator& sim, const std::string& pin) const {
        const NetId net = nets.at(pin);
        return sim.isFloating(net) ? Z : sim.value(net);
    }
};

}  // namespace

TEST_CASE("74LS244: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74LS244");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-20");
    CHECK(part->kind == PartKind::Chip);
    Ls244 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 20);
    const std::array<std::string_view, 20> names{
        "1OE_N", "1A1", "2Y4", "1A2", "2Y3", "1A3", "2Y2", "1A4", "2Y1",   "GND",
        "2A1",   "1Y4", "2A2", "1Y3", "2A3", "1Y2", "2A4", "1Y1", "2OE_N", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
        const PinKind kind = names[i].starts_with("GND") || names[i] == "VCC" ? PinKind::Power
                             : names[i][1] == 'Y'                             ? PinKind::Output
                                                                              : PinKind::Input;
        CHECK(pins[i].kind == kind);
    }
    CHECK(pins[Ls244::k1OeN].name == "1OE_N");
    CHECK(pins[Ls244::k2OeN].name == "2OE_N");
}

TEST_CASE("74LS244: each buffer, enabled and disabled", "[parts]") {
    const int group = GENERATE(1, 2);
    const int unit = GENERATE(1, 2, 3, 4);
    INFO(group << "Y" << unit);
    const std::string g = std::to_string(group);
    const std::string other = std::to_string(3 - group);
    const std::string a = g + "A" + std::to_string(unit);
    const std::string y = g + "Y" + std::to_string(unit);
    const std::string otherY = other + "Y" + std::to_string(unit);

    SECTION("from 1") {
        // A 1; OE_N low at 100 ns; A falls at 200 ns; OE_N high at 300 ns; A rises at 350 ns;
        // OE_N low at 400 ns.
        Bench b({{g + "OE_N", {One, {{100_ns, Zero}, {300_ns, One}, {400_ns, Zero}}}},
                 {a, {One, {{200_ns, Zero}, {350_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        CHECK(b.value(sim, y) == Z);
        sim.runUntil(100_ns + kLs244Enable.rise - 1);
        CHECK(b.value(sim, y) == Z);
        sim.runUntil(100_ns + kLs244Enable.rise);
        CHECK(b.value(sim, y) == One);
        sim.runUntil(200_ns + kLs244Data.fall - 1);
        CHECK(b.value(sim, y) == One);
        sim.runUntil(200_ns + kLs244Data.fall);
        CHECK(b.value(sim, y) == Zero);
        sim.runUntil(300_ns + kLs244Disable.fall - 1);
        CHECK(b.value(sim, y) == Zero);
        sim.runUntil(300_ns + kLs244Disable.fall);
        CHECK(b.value(sim, y) == Z);
        sim.runUntil(400_ns + kLs244Enable.rise);
        CHECK(b.value(sim, y) == One);
        CHECK(b.value(sim, otherY) == Z);  // the other group stays off
    }
    SECTION("from 0") {
        // A 0; OE_N low at 100 ns; A rises at 200 ns; OE_N high at 300 ns.
        Bench b(
            {{g + "OE_N", {One, {{100_ns, Zero}, {300_ns, One}}}}, {a, {Zero, {{200_ns, One}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(2);
        sim.runUntil(100_ns + kLs244Enable.fall - 1);
        CHECK(b.value(sim, y) == Z);
        sim.runUntil(100_ns + kLs244Enable.fall);
        CHECK(b.value(sim, y) == Zero);
        sim.runUntil(200_ns + kLs244Data.rise);
        CHECK(b.value(sim, y) == One);
        sim.runUntil(300_ns + kLs244Disable.rise - 1);
        CHECK(b.value(sim, y) == One);
        sim.runUntil(300_ns + kLs244Disable.rise);
        CHECK(b.value(sim, y) == Z);
    }
}

TEST_CASE("74LS244: X on A or OE_N gives X", "[parts]") {
    Bench b({{"1OE_N", {Zero, {{100_ns, X}}}}, {"1A1", {X, {{50_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(40_ns);
    CHECK(b.value(sim, "1Y1") == X);
    sim.runUntil(90_ns);
    CHECK(b.value(sim, "1Y1") == One);
    sim.runUntil(200_ns);
    CHECK(b.value(sim, "1Y1") == X);
    CHECK(b.value(sim, "1Y2") == X);
}
