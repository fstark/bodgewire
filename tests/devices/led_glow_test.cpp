#include "devices/led_glow.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;
using Catch::Approx;

namespace {

using enum LogicValue;

LedGlow Lit() {
    LedGlow glow;
    glow.setAnode({0, One});
    glow.setCathode({0, Zero});
    return glow;
}

}  // namespace

TEST_CASE("led glow: dark before the first window", "[devices]") {
    LedGlow glow = Lit();
    glow.advanceTo(19_ms);
    CHECK(glow.brightness() == 0.0f);
}

TEST_CASE("led glow: steadily lit reads 1", "[devices]") {
    LedGlow glow = Lit();
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 1.0f);
}

TEST_CASE("led glow: floating cathode stays dark", "[devices]") {
    LedGlow glow;
    glow.setAnode({0, One});
    glow.setCathode({0, Zero, true});
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 0.0f);
}

TEST_CASE("led glow: floating anode stays dark", "[devices]") {
    LedGlow glow;
    glow.setAnode({0, One, true});
    glow.setCathode({0, Zero});
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 0.0f);
}

TEST_CASE("led glow: anode low stays dark", "[devices]") {
    LedGlow glow;
    glow.setAnode({0, Zero});
    glow.setCathode({0, Zero});
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 0.0f);
}

TEST_CASE("led glow: 1 kHz square wave reads half", "[devices]") {
    LedGlow glow;
    glow.setAnode({0, One});
    LogicValue k = Zero;
    for (Time t = 0; t < 41_ms; t += 500_us) {
        glow.setCathode({t, k});
        k = k == Zero ? One : Zero;
    }
    glow.advanceTo(41_ms);
    CHECK(glow.brightness() == Approx(0.5).margin(0.001));
}

TEST_CASE("led glow: an edge inside a window splits it", "[devices]") {
    LedGlow glow;
    glow.setAnode({0, One});
    glow.setCathode({0, One});
    glow.setCathode({5_ms, Zero});
    glow.advanceTo(20_ms);
    CHECK(glow.brightness() == Approx(0.75));
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 1.0f);
}

TEST_CASE("led glow: reset goes dark", "[devices]") {
    LedGlow glow = Lit();
    glow.advanceTo(40_ms);
    REQUIRE(glow.brightness() == 1.0f);
    glow.reset();
    CHECK(glow.brightness() == 0.0f);
    glow.advanceTo(40_ms);
    CHECK(glow.brightness() == 0.0f);
}
