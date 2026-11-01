#include "parts/button.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// GND, a pulled-up signal Net and SW1 between them, its pad 1 on the signal.
struct PullUpButton {
    Circuit circuit;
    NetId gnd{}, signal{};
    Button* button = nullptr;

    explicit PullUpButton(bool withOutputHigh = false, LogicValue rail = Zero) {
        gnd = circuit.addNet("GND");
        signal = circuit.addNet("BTN_N");
        circuit.tie(gnd, Zero, DriveStrength::Supply);
        circuit.tie(signal, rail == Zero ? One : Zero, DriveStrength::Weak);
        button = static_cast<Button*>(*FindPart("BUTTON")->create(circuit, "SW1", {}));
        button->attach(0, rail);
        circuit.connect(*button, "1", signal);
        if (withOutputHigh) {
            auto& out = circuit.add<Stimulus>("U1", One, std::vector<Stimulus::Step>{});
            circuit.connect(out, Stimulus::kOut, signal);
        }
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("button: catalogue entry", "[parts]") {
    const PartInfo* info = FindPart("BUTTON");
    REQUIRE(info != nullptr);
    CHECK(info->kind == PartKind::Button);
    CHECK(info->package == "TACT-6x3.5");
    Circuit circuit;
    const std::vector<PartParam> params{{"colour", "red"}};
    const auto created = info->create(circuit, "SW1", params);
    REQUIRE_FALSE(created.has_value());
    CHECK(created.error() == "BUTTON takes no parameters");
}

TEST_CASE("button: pins are TTL outputs", "[parts]") {
    Circuit circuit;
    const auto pins = (*FindPart("BUTTON")->create(circuit, "SW1", {}))->pins();
    REQUIRE(pins.size() == 2);
    CHECK(pins[0].name == "1");
    CHECK(pins[1].name == "2");
    for (const PinSpec& pin : pins) {
        CHECK(pin.kind == PinKind::Output);
        CHECK(pin.family == Family::Ttl);
    }
}

TEST_CASE("button: released drives nothing, pressed pulls to the rail", "[parts]") {
    PullUpButton bench;
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.record(bench.signal);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == One);
    CHECK_FALSE(sim.isFloating(bench.signal));

    bench.button->setPressed(true);
    sim.wake(bench.button->id());
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == Zero);
    bench.button->setPressed(false);
    sim.wake(bench.button->id());
    sim.runUntil(3_us);
    CHECK(sim.value(bench.signal) == One);

    const auto samples = sim.recorder().samples(bench.signal);
    REQUIRE(samples.size() == 3);
    CHECK(samples[1].time == 1_us + kButtonDelay);
    CHECK(samples[1].value == Zero);
    CHECK(samples[2].time == 2_us + kButtonDelay);
    CHECK(samples[2].value == One);
}

TEST_CASE("button: a VCC rail drives 1", "[parts]") {
    PullUpButton bench(false, One);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == Zero);
    bench.button->setPressed(true);
    sim.wake(bench.button->id());
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == One);
}

TEST_CASE("button: pressed against a TTL output high reads 0", "[parts]") {
    PullUpButton bench(true);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.signal) == One);
    bench.button->setPressed(true);
    sim.wake(bench.button->id());
    sim.runUntil(2_us);
    CHECK(sim.value(bench.signal) == Zero);
}

TEST_CASE("button: released after power-on for every seed", "[parts]") {
    PullUpButton bench;
    Simulator sim(bench.circuit);
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        sim.powerOn(seed);
        CHECK(sim.value(bench.signal) == One);
        sim.runUntil(1_us);
        CHECK(sim.value(bench.signal) == One);
    }
}
