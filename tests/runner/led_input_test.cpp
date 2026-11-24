#include "runner/led_input.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "blinker.hpp"

using namespace bw;
using namespace bw::literals;
using Catch::Approx;
using test::Blinker;

namespace {

// To `end` a millisecond at a time, the lamps fed and the recordings trimmed: bw run's loop.
void RunTo(Simulator& sim, LedInput& leds, Time end) {
    while (sim.now() < end) {
        sim.runUntil(std::min(end, sim.now() + 1_ms));
        leds.update();
        sim.trimRecordings();
    }
}

}  // namespace

TEST_CASE("led input: square wave glows half", "[runner]") {
    Blinker blinker;
    Simulator sim(blinker.circuit);
    sim.powerOn(1);
    LedInput leds(sim, {{"D1", 0, {blinker.a, std::nullopt}, {blinker.k, std::nullopt}}});
    REQUIRE(leds.leds().size() == 1);
    CHECK(leds.brightness(0) == 0.0f);
    RunTo(sim, leds, 19_ms);
    CHECK(leds.brightness(0) == 0.0f);  // no 20 ms window closed yet
    RunTo(sim, leds, 100_ms);
    CHECK(leds.brightness(0) == Approx(0.5).margin(0.01));
}

TEST_CASE("led input: Nets that never change light their lamp", "[runner]") {
    // A is pulled up and GND is the rail from power-on: no change is ever recorded on them.
    Blinker blinker;
    Simulator sim(blinker.circuit);
    sim.powerOn(1);
    LedInput leds(sim, {{"lit", 0, {blinker.a, std::nullopt}, {blinker.gnd, std::nullopt}},
                        {"reversed", 0, {blinker.gnd, std::nullopt}, {blinker.a, std::nullopt}}});
    RunTo(sim, leds, 40_ms);
    CHECK(leds.brightness(0) == Approx(1.0f));
    CHECK(leds.brightness(1) == 0.0f);
}

TEST_CASE("led input: a millisecond at a time as in one run", "[runner]") {
    Blinker sliced(true), whole(true);
    const auto lamps = [](const Blinker& b) {
        return std::vector<LedNets>{{"D1", 0, {b.a, std::nullopt}, {b.k, std::nullopt}},
                                    {"D2", 0, {b.k, std::nullopt}, {b.gnd, std::nullopt}}};
    };
    Simulator simSliced(sliced.circuit), simWhole(whole.circuit);
    simSliced.powerOn(1);
    simWhole.powerOn(1);
    LedInput ledsSliced(simSliced, lamps(sliced)), ledsWhole(simWhole, lamps(whole));
    RunTo(simSliced, ledsSliced, 50_ms + 300_us);
    simWhole.runUntil(50_ms + 300_us);
    ledsWhole.update();
    CHECK(ledsSliced.brightness(0) == ledsWhole.brightness(0));
    CHECK(ledsSliced.brightness(1) == ledsWhole.brightness(1));
    CHECK(ledsWhole.brightness(0) > 0.0f);
}
