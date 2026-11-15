#include "parts/mc3446.hpp"

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

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// One MC3446; driver inputs and enables driven by Stimuli (drivers high, enables on), buses and
// receiver outputs on nets. `outside` drives bus pins from another chip (Z when released).
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    explicit Bench(std::map<std::string, Input> inputs, std::map<std::string, Input> outside = {}) {
        for (const char* pin : {"DA", "DB", "DC", "DD"}) {
            inputs.try_emplace(pin, Input{One, {}});
        }
        for (const char* pin : {"ENABC_N", "END_N"}) {
            inputs.try_emplace(pin, Input{Zero, {}});
        }
        auto& chip = circuit.add<Mc3446>("U1");
        for (auto& [pin, input] : inputs) {
            const NetId net = circuit.addNet(pin);
            auto& source = circuit.add<Stimulus>("S" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[pin] = net;
        }
        for (const char* pin : {"BUSA", "BUSB", "BUSC", "BUSD", "RA", "RB", "RC", "RD"}) {
            nets[pin] = circuit.addNet(pin);
            circuit.connect(chip, pin, nets[pin]);
        }
        for (auto& [pin, input] : outside) {
            auto& source = circuit.add<Stimulus>("X" + pin, input.initial, std::move(input.steps));
            circuit.connect(source, Stimulus::kOut, nets[pin]);
        }
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("MC3446: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("MC3446");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    Mc3446 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    const std::vector<std::string> names{"RA", "BUSA", "DA", "ENABC_N", "DB", "BUSB", "RB", "GND",
                                         "RC", "BUSC", "DC", "END_N",   "DD", "BUSD", "RD", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        CHECK(pins[i].name == names[i]);
    }
    CHECK(pins[1].kind == PinKind::PullUp);
    CHECK(pins[0].kind == PinKind::Output);
}

TEST_CASE("MC3446: each driver pulls its bus low and lets go", "[parts]") {
    const auto [d, bus, r] =
        GENERATE(std::tuple{"DA", "BUSA", "RA"}, std::tuple{"DB", "BUSB", "RB"},
                 std::tuple{"DC", "BUSC", "RC"}, std::tuple{"DD", "BUSD", "RD"});
    INFO(d);
    Bench b({{d, {One, {{100_ns, Zero}, {300_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets[bus]) == One);
    CHECK(sim.value(b.nets[r]) == One);
    sim.runUntil(100_ns + delays::kMc3446Driver.fall - 1);
    CHECK(sim.value(b.nets[bus]) == One);
    sim.runUntil(100_ns + delays::kMc3446Driver.fall);
    CHECK(sim.value(b.nets[bus]) == Zero);
    sim.runUntil(100_ns + delays::kMc3446Driver.fall + delays::kMc3446Receiver.fall);
    CHECK(sim.value(b.nets[r]) == Zero);
    sim.runUntil(300_ns + delays::kMc3446Driver.rise);
    CHECK(sim.value(b.nets[bus]) == One);
}

TEST_CASE("MC3446: an enable high releases its channels", "[parts]") {
    Bench b({{"DA", {Zero, {}}}, {"DD", {Zero, {}}}, {"ENABC_N", {Zero, {{100_ns, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["BUSA"]) == Zero);
    CHECK(sim.value(b.nets["BUSD"]) == Zero);
    sim.runUntil(100_ns + delays::kMc3446Enable.rise);
    CHECK(sim.value(b.nets["BUSA"]) == One);
    CHECK(sim.value(b.nets["BUSD"]) == Zero);  // END_N still low
}

TEST_CASE("MC3446: a receiver follows a bus pulled low from outside", "[parts]") {
    Bench b({}, {{"BUSB", {Z, {{100_ns, Zero}, {200_ns, Z}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(90_ns);
    CHECK(sim.value(b.nets["RB"]) == One);
    sim.runUntil(100_ns + delays::kMc3446Receiver.fall);
    CHECK(sim.value(b.nets["BUSB"]) == Zero);
    CHECK(sim.value(b.nets["RB"]) == Zero);
    sim.runUntil(200_ns + delays::kMc3446Receiver.rise);
    CHECK(sim.value(b.nets["RB"]) == One);
}

TEST_CASE("MC3446: X on a driver gives X on its bus", "[parts]") {
    Bench b({{"DC", {X, {}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(200_ns);
    CHECK(sim.value(b.nets["BUSC"]) == X);
    CHECK(sim.value(b.nets["BUSA"]) == One);
}
