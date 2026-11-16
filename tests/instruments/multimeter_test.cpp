#include <filesystem>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "instruments/multimeter.hpp"
#include "instruments/volts.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;
using Catch::Approx;

TEST_CASE("meter: 25 % duty reads 1.03 V", "[instruments]") {
    MeterState meter;
    meter.reset();
    for (Time t = 0; t < 1_s; t += 1_us) {
        meter.feed({t, LogicValue::One});
        meter.feed({t + 250_ns, LogicValue::Zero});
    }
    meter.advanceTo(1_s);
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(1.025).margin(0.001));
}

TEST_CASE("meter: no reading before the first refresh", "[instruments]") {
    MeterState meter;
    meter.feed({0, LogicValue::One});
    meter.advanceTo(249_ms);
    CHECK_FALSE(meter.reading().has_value());
    meter.advanceTo(250_ms);
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(kHighVolts));
    meter.reset();
    CHECK_FALSE(meter.reading().has_value());
}

TEST_CASE("meter: window starts at the first Sample", "[instruments]") {
    MeterState meter;
    meter.feed({100_ms, LogicValue::One});
    meter.advanceTo(250_ms);
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(3.5));
    meter.feed({350_ms, LogicValue::Zero});
    meter.advanceTo(500_ms);
    CHECK(*meter.reading() == Approx((50 * 3.5 + 150 * 0.2) / 200));
}

TEST_CASE("meter: test pattern CLK", "[instruments]") {
    const auto loaded = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(loaded.has_value());
    Simulator sim((*loaded)->circuit);
    sim.powerOn(1);
    Multimeter meter(sim, (*loaded)->nets.at("CLK"));
    while (sim.now() < 1_s) {
        sim.runUntil(sim.now() + 1_ms);
        meter.update();
    }
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(1.85).margin(0.01));
}

TEST_CASE("volts: logic states", "[instruments]") {
    CHECK(LogicVolts({0, LogicValue::Zero}) == kLowVolts);
    CHECK(LogicVolts({0, LogicValue::One}) == kHighVolts);
    CHECK(LogicVolts({0, LogicValue::Z, true}) == kFloatingVolts);
    CHECK(LogicVolts({0, LogicValue::One, true}) == kFloatingVolts);
    CHECK(LogicVolts({0, LogicValue::X}) == kUnknownVolts);
    CHECK(kLowVolts == 0.2);
    CHECK(kHighVolts == 3.5);
    CHECK(kFloatingVolts == 1.4);
    CHECK(kUnknownVolts == 1.0);
}
