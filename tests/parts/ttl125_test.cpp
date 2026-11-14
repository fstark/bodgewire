#include "parts/ttl125.hpp"

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

// One 74125; every input driven by a Stimulus (listed, or 0 for an A, 1 for an OE_N), every
// output on a Net of its own.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(const std::map<std::string, Input>& inputs) {
        auto& chip = circuit.add<Ttl125>("U1");
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

TEST_CASE("74125: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("74125");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-14");
    CHECK(part->kind == PartKind::Chip);
    Ttl125 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 14);
    const std::array<std::string_view, 14> names{"1OE_N", "1A",  "1Y",    "2OE_N", "2A",
                                                 "2Y",    "GND", "3Y",    "3A",    "3OE_N",
                                                 "4Y",    "4A",  "4OE_N", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
        const PinKind kind = names[i] == "GND" || names[i] == "VCC" ? PinKind::Power
                             : names[i][1] == 'Y'                   ? PinKind::Output
                                                                    : PinKind::Input;
        CHECK(pins[i].kind == kind);
        CHECK(pins[i].family == Family::Ttl);
    }
}

TEST_CASE("74125: each buffer, enabled and disabled", "[parts]") {
    const int unit = GENERATE(1, 2, 3, 4);
    INFO(unit);
    const std::string u = std::to_string(unit);
    const std::string oe = u + "OE_N", a = u + "A", y = u + "Y";
    const std::string other = std::to_string(unit % 4 + 1) + "Y";

    // A 1; OE_N low at 100 ns; A falls at 200 ns; OE_N high at 300 ns; A rises at 350 ns;
    // OE_N low at 400 ns; OE_N high at 500 ns (from 1).
    Bench b({{oe, {One, {{100_ns, Zero}, {300_ns, One}, {400_ns, Zero}, {500_ns, One}}}},
             {a, {One, {{200_ns, Zero}, {350_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    CHECK(b.value(sim, y) == Z);
    sim.runUntil(100_ns + kTtl125Enable.rise - 1);
    CHECK(b.value(sim, y) == Z);
    sim.runUntil(100_ns + kTtl125Enable.rise);
    CHECK(b.value(sim, y) == One);
    CHECK(b.value(sim, other) == Z);  // the other buffers stay off
    sim.runUntil(200_ns + kTtl125Data.fall - 1);
    CHECK(b.value(sim, y) == One);
    sim.runUntil(200_ns + kTtl125Data.fall);
    CHECK(b.value(sim, y) == Zero);
    sim.runUntil(300_ns + kTtl125Disable.fall - 1);
    CHECK(b.value(sim, y) == Zero);
    sim.runUntil(300_ns + kTtl125Disable.fall);
    CHECK(b.value(sim, y) == Z);
    sim.runUntil(400_ns + kTtl125Enable.rise);
    CHECK(b.value(sim, y) == One);
    sim.runUntil(500_ns + kTtl125Disable.rise - 1);
    CHECK(b.value(sim, y) == One);
    sim.runUntil(500_ns + kTtl125Disable.rise);
    CHECK(b.value(sim, y) == Z);
}

TEST_CASE("74125: enabled onto 0, X data and X enable", "[parts]") {
    // 1A 0, enabled at 100 ns; 2A X, enabled; 3OE_N X.
    Bench b({{"1OE_N", {One, {{100_ns, Zero}}}},
             {"2OE_N", {Zero, {}}},
             {"2A", {X, {}}},
             {"3OE_N", {X, {}}},
             {"3A", {One, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns + kTtl125Enable.fall - 1);
    CHECK(b.value(sim, "1Y") == Z);
    sim.runUntil(100_ns + kTtl125Enable.fall);
    CHECK(b.value(sim, "1Y") == Zero);
    CHECK(b.value(sim, "2Y") == X);
    CHECK(b.value(sim, "3Y") == X);
}
