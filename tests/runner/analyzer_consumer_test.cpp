#include "runner/analyzer_consumer.hpp"

#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/builder.hpp"
#include "runner/runner.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::unique_ptr<LoadedCircuit> LoadTestPattern() {
    auto result = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

void RunTo(Runner& runner, Time time) {
    runner.resumeUntil(time);
    runner.waitIdle();
}

AnalyzerConfig Armed(std::array<std::optional<NetId>, kChannels> nets, AnalyzerSettings settings,
                     std::uint64_t arms = 1) {
    return {.nets = nets, .labels = {}, .settings = settings, .arms = arms, .armed = true};
}

// Every point from `first` on `channel` is a known 1.
bool AllHigh(const Capture& capture, std::size_t channel) {
    for (std::size_t p = capture.first; p < capture.points.size(); ++p) {
        if ((capture.points[p].unknown >> channel & 1) != 0 ||
            (capture.points[p].high >> channel & 1) == 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("analyzer consumer: capture through the runner", "[runner]") {
    auto loaded = LoadTestPattern();
    const NetId lineEnd = loaded->nets.at("LINE_END_N");
    const NetId clk = loaded->nets.at("CLK");
    AnalyzerSettings settings{.period = 10_ns};
    settings.trigger[0] = TriggerCondition::Fall;

    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    LogicAnalyzer reference(sim, {lineEnd, clk});
    reference.arm(settings);
    while (reference.state().phase() != AnalyzerPhase::Captured && sim.now() < 2_ms) {
        sim.runUntil(sim.now() + 1_ms);
        reference.update();
    }
    REQUIRE(reference.state().phase() == AnalyzerPhase::Captured);

    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    runner.setSpeed(Speed::Max);
    analyzer.configure(runner, Armed({lineEnd, clk}, settings));
    runner.pauseAt(2_ms);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(analyzer.captures().update());
    const Capture& capture = analyzer.captures().front();
    const Capture& expected = reference.state().capture();
    CHECK(capture.points == expected.points);
    CHECK(capture.trigger == expected.trigger);
    CHECK(capture.triggerTime == expected.triggerTime);
}

TEST_CASE("analyzer consumer: armed before power-on", "[runner]") {
    auto loaded = LoadTestPattern();
    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    runner.setSpeed(Speed::Max);
    analyzer.configure(runner, Armed({loaded->nets.at("CLK")}, {.period = 10_ns}));
    CHECK(analyzer.phase() == AnalyzerPhase::Armed);
    runner.pauseAt(2_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(analyzer.phase() == AnalyzerPhase::Captured);
}

TEST_CASE("analyzer consumer: reconfigure while armed restarts", "[runner]") {
    auto loaded = LoadTestPattern();
    const NetId lineEnd = loaded->nets.at("LINE_END_N");
    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    runner.setSpeed(Speed::Max);
    AnalyzerSettings impossible{.period = 10_ns};
    impossible.trigger[0] = TriggerCondition::High;
    impossible.trigger[1] = TriggerCondition::Low;
    analyzer.configure(runner, Armed({lineEnd, lineEnd}, impossible));
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(analyzer.phase() == AnalyzerPhase::Armed);

    AnalyzerSettings possible{.period = 10_ns};
    possible.trigger[0] = TriggerCondition::Fall;
    analyzer.configure(runner, Armed({lineEnd, lineEnd}, possible));
    RunTo(runner, 2_ms);
    REQUIRE(analyzer.phase() == AnalyzerPhase::Captured);
    REQUIRE(analyzer.captures().update());
    CHECK(analyzer.captures().front().triggerTime >= 1_ms);
}

TEST_CASE("analyzer consumer: new clip gets its initial value", "[runner]") {
    auto loaded = LoadTestPattern();
    const auto clear = PinNet(*loaded, "U1", "1");
    REQUIRE(clear.has_value());
    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    analyzer.configure(runner, Armed({clear}, {.period = 10_ns}));
    RunTo(runner, 2_ms);
    REQUIRE(analyzer.captures().update());
    CHECK(AllHigh(analyzer.captures().front(), 0));
}

TEST_CASE("analyzer consumer: second clip on a watched net", "[runner]") {
    auto loaded = LoadTestPattern();
    const auto clear = PinNet(*loaded, "U1", "1");
    REQUIRE(clear.has_value());
    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    runner.setSpeed(Speed::Max);
    analyzer.configure(runner, {.nets = {clear}});
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    analyzer.configure(runner, Armed({clear, clear}, {.period = 10_ns}));
    RunTo(runner, 2_ms);
    REQUIRE(analyzer.captures().update());
    CHECK(AllHigh(analyzer.captures().front(), 1));
}

TEST_CASE("analyzer consumer: a late power-off does not undo an Arm", "[runner]") {
    auto loaded = LoadTestPattern();
    AnalyzerConsumer analyzer;
    Runner runner(loaded->circuit, {&analyzer});
    const AnalyzerConfig armed = Armed({loaded->nets.at("CLK")}, {.period = 10_ns});
    analyzer.configure(runner, armed);
    runner.waitIdle();
    analyzer.powerOff();  // the devices thread, before it applied the Arm
    CHECK(analyzer.phase() == AnalyzerPhase::Armed);

    AnalyzerConfig stopped = armed;
    stopped.armed = false;
    analyzer.configure(runner, stopped);
    analyzer.powerOff();
    CHECK(analyzer.phase() == AnalyzerPhase::Idle);
}
