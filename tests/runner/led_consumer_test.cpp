#include "runner/led_consumer.hpp"

#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "blinker.hpp"
#include "runner/runner.hpp"

using namespace bw;
using namespace bw::literals;
using Catch::Approx;
using test::Blinker;

namespace {

using enum LogicValue;

}  // namespace

TEST_CASE("led consumer: watches its Nets", "[runner]") {
    const NetId a{1}, k{2}, gnd{3};
    const LedConsumer one({{"D1", 0, {a, std::nullopt}, {k, std::nullopt}}});
    CHECK(std::vector(one.nets().begin(), one.nets().end()) == std::vector{a, k});
    const LedConsumer two({{"D1", 0, {a, std::nullopt}, {k, std::nullopt}},
                           {"D2", 0, {k, std::nullopt}, {gnd, std::nullopt}},
                           {"D3", 0, {a, std::nullopt}, {k, std::nullopt}}});
    CHECK(std::vector(two.nets().begin(), two.nets().end()) == std::vector{a, k, gnd});
    CHECK(two.leds().size() == 3);
    CHECK(two.brightness(2) == 0.0f);
}

TEST_CASE("led consumer: a lamp with two cathode Nets", "[runner]") {
    const NetId a{1}, k1{2}, k2{3};
    const LedConsumer leds({{"DS1", 0, {a, std::nullopt}, {k1, k2}}});
    CHECK(std::vector(leds.nets().begin(), leds.nets().end()) == std::vector{a, k1, k2});
}

TEST_CASE("led consumer: square wave glows half", "[runner]") {
    Blinker blinker;
    LedConsumer leds({{"D1", 0, {blinker.a, std::nullopt}, {blinker.k, std::nullopt}}});
    Runner runner(blinker.circuit, {&leds});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(100_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(leds.brightness(0) == Approx(0.5).margin(0.01));
}

TEST_CASE("led consumer: dark after power off", "[runner]") {
    Blinker blinker;
    LedConsumer leds({{"D1", 0, {blinker.a, std::nullopt}, {blinker.k, std::nullopt}}});
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
    LedConsumer leds({{"D1", 0, {blinker.a, std::nullopt}, {blinker.k, std::nullopt}},
                      {"D2", 0, {blinker.k, std::nullopt}, {blinker.gnd, std::nullopt}}});
    CHECK(leds.nets().size() == 3);
    Runner runner(blinker.circuit, {&leds});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(100_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(leds.brightness(0) == Approx(0.5).margin(0.01));
    CHECK(leds.brightness(1) == Approx(0.5).margin(0.01));
}

TEST_CASE("led consumer: digit lamps share their anode Nets", "[runner]") {
    const NetId ca{1}, a{2}, b{3};
    LedConsumer digit(
        {{"DS1", 0, {ca, ca}, {a, std::nullopt}}, {"DS1", 1, {ca, ca}, {b, std::nullopt}}});
    CHECK(std::vector(digit.nets().begin(), digit.nets().end()) == std::vector{ca, a, b});
    digit.powerOn();
    digit.sample(ca, {0, One});
    digit.sample(a, {0, Zero});
    digit.sample(b, {0, One});
    digit.advanceTo(kLedWindow);
    CHECK(digit.brightness(0) == Approx(1.0f));
    CHECK(digit.brightness(1) == 0.0f);
}
