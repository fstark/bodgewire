#include "runner/meter_consumer.hpp"

#include <filesystem>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "instruments/volts.hpp"
#include "netlist/builder.hpp"
#include "runner/runner.hpp"

using namespace bw;
using namespace bw::literals;
using Catch::Approx;

namespace {

std::unique_ptr<LoadedCircuit> LoadTestPattern() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

void RunTo(Runner& runner, Time time) {
    runner.pauseAt(time);
    runner.resume();
    runner.waitIdle();
}

}  // namespace

TEST_CASE("meter consumer: reading through the runner", "[runner]") {
    auto loaded = LoadTestPattern();
    const NetId clk = loaded->nets.at("CLK");

    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    Multimeter reference(sim, clk);
    while (sim.now() < 1_s) {
        sim.runUntil(sim.now() + 1_ms);
        reference.update();
    }
    REQUIRE(reference.reading().has_value());

    MeterConsumer meter;
    Runner runner(loaded->circuit, {&meter});
    runner.setSpeed(Speed::Max);
    meter.touch(runner, clk);
    CHECK_FALSE(meter.reading().has_value());
    runner.pauseAt(1_s);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(*reference.reading()).margin(0.01));
}

TEST_CASE("meter consumer: retarget resets", "[runner]") {
    auto loaded = LoadTestPattern();
    MeterConsumer meter;
    Runner runner(loaded->circuit, {&meter});
    runner.setSpeed(Speed::Max);
    meter.touch(runner, loaded->nets.at("CLK"));
    runner.pauseAt(500_ms);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(1.85).margin(0.01));

    meter.touch(runner, loaded->nets.at("VCC"));
    CHECK_FALSE(meter.reading().has_value());
    RunTo(runner, 800_ms);
    REQUIRE(meter.reading().has_value());
    CHECK(*meter.reading() == Approx(kHighVolts));

    meter.touch(runner, std::nullopt);
    CHECK_FALSE(meter.reading().has_value());
}
