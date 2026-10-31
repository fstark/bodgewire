#include "runner/scope_consumer.hpp"

#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "runner/runner.hpp"

using namespace bw;
using namespace bw::literals;

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

TEST_CASE("scope consumer: sweep through the runner", "[runner]") {
    auto loaded = LoadTestPattern();
    const NetId clk = loaded->nets.at("CLK");
    const NetId h0 = loaded->nets.at("H0");
    const ScopeSettings settings{.timebase = 1_us};

    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    Oscilloscope reference(sim, {clk, h0}, settings);
    while (sim.now() < 2_ms) {
        sim.runUntil(sim.now() + 1_ms);
        reference.update();
    }
    REQUIRE(reference.state().sweep().serial > 0);

    ScopeConsumer scope;
    Runner runner(loaded->circuit, {&scope});
    runner.setSpeed(Speed::Max);
    scope.configure(runner, {.nets = {clk, h0}, .settings = settings});
    runner.pauseAt(2_ms);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(scope.sweeps().update());
    const Sweep& sweep = scope.sweeps().front();
    const Sweep& expected = reference.state().sweep();
    CHECK(scope.status() == SweepKind::Triggered);
    CHECK(sweep.kind == SweepKind::Triggered);
    CHECK(sweep.start == expected.start);
    CHECK(sweep.channels == expected.channels);
}

TEST_CASE("scope consumer: second hook on a watched net", "[runner]") {
    auto loaded = LoadTestPattern();
    const auto clear = PinNet(*loaded, "U1", "1");
    REQUIRE(clear.has_value());
    ScopeConsumer scope;
    Runner runner(loaded->circuit, {&scope});
    runner.setSpeed(Speed::Max);
    scope.configure(runner, {.nets = {clear}});
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    scope.configure(runner, {.nets = {clear, clear}});
    CHECK(scope.status() == SweepKind::None);
    RunTo(runner, 60_ms);
    REQUIRE(scope.sweeps().update());
    const Sweep& sweep = scope.sweeps().front();
    CHECK(sweep.kind == SweepKind::Auto);
    REQUIRE_FALSE(sweep.channels[1].empty());
    CHECK(sweep.channels[1].front().value == LogicValue::One);
}
