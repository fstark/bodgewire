#include "runner/led_consumer.hpp"

#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "runner/runner.hpp"

using namespace bw;
using namespace bw::literals;
using Catch::Approx;

namespace {

using enum LogicValue;

// X1 1 kHz on K; A pulled up; D1 between them. `second`: D2 from K (anode) to GND (cathode).
struct Blinker {
    Circuit circuit;
    NetId gnd, vcc, a, k;

    explicit Blinker(bool second = false) {
        gnd = circuit.addNet("GND");
        vcc = circuit.addNet("VCC");
        circuit.tie(gnd, Zero, DriveStrength::Supply);
        circuit.tie(vcc, One, DriveStrength::Supply);
        const std::vector<PartParam> freq{{"freq", "1kHz"}};
        Component& x1 = *FindPart("OSC")->create(circuit, "X1", freq).value();
        Component& d1 = *FindPart("LED")->create(circuit, "D1", {}).value();
        a = circuit.addNet("A");
        k = circuit.addNet("K");
        circuit.tie(a, One, DriveStrength::Weak);
        circuit.connect(x1, "GND", gnd);
        circuit.connect(x1, "VCC", vcc);
        circuit.connect(x1, "OUT", k);
        circuit.connect(d1, "A", a);
        circuit.connect(d1, "K", k);
        if (second) {
            Component& d2 = *FindPart("LED")->create(circuit, "D2", {}).value();
            circuit.connect(d2, "A", k);
            circuit.connect(d2, "K", gnd);
        }
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("led consumer: watches its Nets", "[runner]") {
    const NetId a{1}, k{2}, gnd{3};
    const LedConsumer one({{"D1", a, k}});
    CHECK(std::vector(one.nets().begin(), one.nets().end()) == std::vector{a, k});
    const LedConsumer two({{"D1", a, k}, {"D2", k, gnd}, {"D3", a, k}});
    CHECK(std::vector(two.nets().begin(), two.nets().end()) == std::vector{a, k, gnd});
    CHECK(two.leds().size() == 3);
    CHECK(two.brightness(2) == 0.0f);
}

TEST_CASE("led consumer: square wave glows half", "[runner]") {
    Blinker blinker;
    LedConsumer leds({{"D1", blinker.a, blinker.k}});
    Runner runner(blinker.circuit, {&leds});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(100_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(leds.brightness(0) == Approx(0.5).margin(0.01));
}

TEST_CASE("led consumer: dark after power off", "[runner]") {
    Blinker blinker;
    LedConsumer leds({{"D1", blinker.a, blinker.k}});
    Runner runner(blinker.circuit, {&leds});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(50_ms);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(leds.brightness(0) > 0.0f);
    runner.powerOff();
    runner.waitIdle();
    CHECK(leds.brightness(0) == 0.0f);
}

TEST_CASE("led consumer: one Net feeds two LEDs", "[runner]") {
    Blinker blinker(true);
    LedConsumer leds({{"D1", blinker.a, blinker.k}, {"D2", blinker.k, blinker.gnd}});
    CHECK(leds.nets().size() == 3);
    Runner runner(blinker.circuit, {&leds});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(100_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(leds.brightness(0) == Approx(0.5).margin(0.01));
    CHECK(leds.brightness(1) == Approx(0.5).margin(0.01));
}
