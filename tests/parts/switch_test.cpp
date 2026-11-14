#include "parts/switch.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

Switch& Create(Circuit& circuit, std::string_view part, std::vector<PartParam> params = {}) {
    const auto created = FindPart(part)->create(circuit, "SW1", params);
    REQUIRE(created.has_value());
    return static_cast<Switch&>(**created);
}

std::string CreateError(std::string_view part, std::vector<PartParam> params) {
    Circuit circuit;
    const auto created = FindPart(part)->create(circuit, "SW1", params);
    REQUIRE_FALSE(created.has_value());
    return created.error();
}

// A rail and a pulled Net with SW1 between them, joined by a SwitchNetwork as the builder does.
struct PullUpButton {
    Circuit circuit;
    NetId rail{}, signal{};
    Switch* button = nullptr;

    explicit PullUpButton(bool withOutputHigh = false, LogicValue railValue = Zero,
                          SwitchFault fault = SwitchFault::None) {
        rail = circuit.addNet(railValue == Zero ? "GND" : "VCC");
        signal = circuit.addNet("BTN_N");
        circuit.tie(rail, railValue, DriveStrength::Supply);
        circuit.tie(signal, Not(railValue), DriveStrength::Weak);
        button = &Create(circuit, "BUTTON");
        button->setFault(fault);
        circuit.connect(*button, "1", signal);
        const std::vector<std::string> nets{"RAIL", "BTN_N"};
        auto& network = circuit.add<SwitchNetwork>(nets);
        circuit.connect(network, 0, rail);
        circuit.connect(network, 1, signal);
        network.add(*button, 1, 0);
        if (withOutputHigh) {
            auto& out = circuit.add<Stimulus>("U1", One, std::vector<Stimulus::Step>{});
            circuit.connect(out, Stimulus::kOut, signal);
        }
        circuit.finalize();
    }

    void press(Simulator& sim, bool pressed) const {
        button->setPressed(pressed);
        sim.wake(button->id());
    }
};

}  // namespace

TEST_CASE("switch: catalogue entries", "[parts]") {
    const PartInfo* button = FindPart("BUTTON");
    REQUIRE(button != nullptr);
    CHECK(button->kind == PartKind::Switch);
    CHECK(button->package == "TACT-6x3.5");
    const PartInfo* key = FindPart("KEY");
    REQUIRE(key != nullptr);
    CHECK(key->kind == PartKind::Switch);
    CHECK(key->package == "MX-1U");
    const PartInfo* slide = FindPart("SLIDE");
    REQUIRE(slide != nullptr);
    CHECK(slide->kind == PartKind::Switch);
    CHECK(slide->package == "SLIDE-1");
}

TEST_CASE("switch: SLIDE takes legend= only", "[parts]") {
    Circuit circuit;
    Switch& sst = Create(circuit, "SLIDE", {{"legend", "SST"}});
    CHECK(sst.legend() == "SST");
    CHECK(sst.pcKey().empty());
    CHECK(Create(circuit, "SLIDE").legend().empty());
    CHECK(Create(circuit, "SLIDE", {{"legend", "S"}}).pcKey().empty());
    CHECK(CreateError("SLIDE", {{"pc", "S"}}) == "SLIDE takes legend=, not pc=");
    CHECK(CreateError("SLIDE", {{"legend", "SINGLE"}}) ==
          "SLIDE legend= takes 1 to 4 printable characters, not 'SINGLE'");
}

TEST_CASE("switch: legend and PC key", "[parts]") {
    Circuit circuit;
    CHECK(Create(circuit, "BUTTON").pcKey().empty());
    CHECK(Create(circuit, "BUTTON", {{"pc", "Space"}}).pcKey().empty() == false);
    Switch& seven = Create(circuit, "KEY", {{"legend", "7"}});
    CHECK(seven.legend() == "7");
    CHECK(seven.pcKey() == "7");
    Switch& go = Create(circuit, "KEY", {{"legend", "GO"}, {"pc", "g"}});
    CHECK(go.legend() == "GO");
    CHECK(go.pcKey() == "G");
    CHECK(Create(circuit, "KEY", {{"legend", "a"}}).pcKey() == "A");
    CHECK(Create(circuit, "KEY", {{"legend", "DA"}}).pcKey().empty());
    CHECK(Create(circuit, "KEY").legend().empty());
    CHECK(Create(circuit, "KEY", {{"legend", "+"}, {"pc", "Enter"}}).pcKey() == "Enter");

    CHECK(CreateError("BUTTON", {{"legend", "A"}}) == "BUTTON takes pc=, not legend=");
    CHECK(CreateError("KEY", {{"colour", "red"}}) == "KEY takes legend= and pc=, not colour=");
    CHECK(CreateError("KEY", {{"legend", "A"}, {"legend", "B"}}) == "KEY: legend= given twice");
    CHECK(CreateError("KEY", {{"legend", "RESET"}}) ==
          "KEY legend= takes 1 to 4 printable characters, not 'RESET'");
    CHECK(CreateError("KEY", {{"pc", "F2"}}).starts_with("KEY: unknown PC key 'F2'"));
}

TEST_CASE("switch: PC key names", "[parts]") {
    for (const std::string_view key : {"A", "z", "7", "+", "Space", "Enter", "Tab", "Backspace",
                                       "Escape", "Up", "Down", "Left", "Right"}) {
        CHECK(IsPcKey(key));
    }
    for (const std::string_view key : {"", " ", "#", "AB", "F2", "space", "\x7f"}) {
        CHECK_FALSE(IsPcKey(key));
    }
}

TEST_CASE("switch: pins are TTL outputs that never drive", "[parts]") {
    Circuit circuit;
    const auto pins = Create(circuit, "KEY").pins();
    REQUIRE(pins.size() == 2);
    CHECK(pins[0].name == "1");
    CHECK(pins[1].name == "2");
    for (const PinSpec& pin : pins) {
        CHECK(pin.kind == PinKind::Output);
        CHECK(pin.family == Family::Ttl);
    }
}

TEST_CASE("switch: released joins nothing, pressed pulls to the rail", "[parts]") {
    PullUpButton bench;
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.record(bench.signal);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == One);
    CHECK_FALSE(sim.isFloating(bench.signal));

    bench.press(sim, true);
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == Zero);
    CHECK(bench.button->closed());
    bench.press(sim, false);
    sim.runUntil(3_us);
    CHECK(sim.value(bench.signal) == One);

    const auto samples = sim.recorder().samples(bench.signal);
    REQUIRE(samples.size() == 3);
    CHECK(samples[1].time == 1_us + kPressLatency);
    CHECK(samples[1].value == Zero);
    CHECK(samples[2].time == 2_us + kPressLatency);
    CHECK(samples[2].value == One);
}

TEST_CASE("switch: a VCC rail drives 1", "[parts]") {
    PullUpButton bench(false, One);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == Zero);
    bench.press(sim, true);
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == One);
}

TEST_CASE("switch: pressed, the rail beats a TTL output high", "[parts]") {
    PullUpButton bench(true);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == One);
    bench.press(sim, true);
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == Zero);
}

TEST_CASE("switch: released after power-on for every seed", "[parts]") {
    PullUpButton bench;
    Simulator sim(bench.circuit);
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        sim.powerOn(seed);
        CHECK(sim.value(bench.signal) == One);
        sim.runUntil(1_us);
        CHECK(sim.value(bench.signal) == One);
    }
}

TEST_CASE("switch: Faults", "[parts]") {
    SECTION("stuck closed: pulled from power-on, a press changes nothing") {
        PullUpButton bench(false, Zero, SwitchFault::StuckClosed);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(sim.value(bench.signal) == Zero);
        bench.press(sim, true);
        sim.runUntil(2_us);
        bench.press(sim, false);
        sim.runUntil(3_us);
        CHECK(sim.value(bench.signal) == Zero);
    }
    SECTION("never closes: a press does nothing") {
        PullUpButton bench(false, Zero, SwitchFault::NeverCloses);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.press(sim, true);
        sim.runUntil(1_us);
        CHECK(sim.value(bench.signal) == One);
        CHECK_FALSE(bench.button->closed());
    }
}

TEST_CASE("switch: a press lands at the switch's wake, whatever else wakes the network",
          "[parts]") {
    // The signal's other driver toggles every 100 ns, waking the network each time; the press,
    // written long before, shows only at the switch's own wake.
    Circuit circuit;
    const NetId gnd = circuit.addNet("GND");
    const NetId a = circuit.addNet("A");
    const NetId b = circuit.addNet("B");
    circuit.tie(gnd, Zero, DriveStrength::Supply);
    circuit.tie(b, One, DriveStrength::Weak);
    std::vector<Stimulus::Step> steps;
    for (Time t = 100_ns; t < 2_us; t += 100_ns) {
        steps.push_back({t, steps.size() % 2 == 0 ? Zero : One});
    }
    auto& toggler = circuit.add<Stimulus>("T", One, steps, Family::Ttl);
    circuit.connect(toggler, Stimulus::kOut, a);
    Switch& joinsAB = Create(circuit, "KEY");
    Switch& grounds = Create(circuit, "KEY");
    const std::vector<std::string> nets{"GND", "A", "B"};
    auto& network = circuit.add<SwitchNetwork>(nets);
    circuit.connect(network, 0, gnd);
    circuit.connect(network, 1, a);
    circuit.connect(network, 2, b);
    network.add(joinsAB, 1, 2);
    network.add(grounds, 0, 2);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    joinsAB.setPressed(true);
    sim.wake(joinsAB.id());
    sim.runUntil(150_ns);
    CHECK(sim.value(b) == Zero);  // A's driver went 0 at 100 ns, through the key
    grounds.setPressed(true);     // not woken until 1 µs
    sim.wake(grounds.id(), 1_us - 150_ns);
    sim.runUntil(1_us - 1);
    CHECK_FALSE(grounds.closed());  // the network woke at every toggle, without the press
    sim.runUntil(1_us + kPressLatency);
    CHECK(grounds.closed());
    CHECK(sim.value(a) == Zero);  // GND, A and B joined: the rail wins
    CHECK(sim.value(b) == Zero);
}
