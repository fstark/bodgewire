#include "runner/probe_consumer.hpp"

#include <filesystem>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "runner/runner.hpp"
#include "runner/tv_consumer.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::unique_ptr<LoadedCircuit> LoadTestPattern() {
    auto result = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

void RunTo(Runner& runner, Time time) {
    runner.pauseAt(time);
    runner.resume();
    runner.waitIdle();
}

std::string Lamps(const ProbeConsumer& probe) { return FormatLamps(probe.lamps()); }

}  // namespace

TEST_CASE("probe consumer: CLK pulses", "[runner]") {
    auto loaded = LoadTestPattern();
    ProbeConsumer probe;
    Runner runner(loaded->circuit, {&probe});
    runner.setSpeed(Speed::Max);
    probe.touch(runner, loaded->nets.at("CLK"));
    runner.pauseAt(10_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK((Lamps(probe) == "HIGH PULSE" || Lamps(probe) == "LOW PULSE"));
}

TEST_CASE("probe consumer: move, lift, power off", "[runner]") {
    auto loaded = LoadTestPattern();
    ProbeConsumer probe;
    Runner runner(loaded->circuit, {&probe});
    runner.setSpeed(Speed::Max);
    CHECK(Lamps(probe) == "-");
    probe.touch(runner, loaded->nets.at("CLK"));
    runner.pauseAt(10_ms);
    runner.powerOn(1);
    runner.waitIdle();

    probe.touch(runner, loaded->nets.at("GND"));
    CHECK(probe.touched() == loaded->nets.at("GND"));
    RunTo(runner, 20_ms);
    CHECK(Lamps(probe) == "LOW");
    RunTo(runner, 200_ms);
    CHECK(Lamps(probe) == "LOW");

    probe.touch(runner, std::nullopt);
    CHECK(Lamps(probe) == "-");
    RunTo(runner, 210_ms);
    CHECK(Lamps(probe) == "-");

    probe.touch(runner, loaded->nets.at("VCC"));
    RunTo(runner, 220_ms);
    CHECK(Lamps(probe) == "HIGH");
    runner.powerOff();
    runner.waitIdle();
    CHECK(Lamps(probe) == "-");
}

TEST_CASE("probe consumer: leaving VIDEO keeps the TV", "[runner]") {
    auto loaded = LoadTestPattern();
    const NetId video = *ConnectorNet(*loaded, "VIDEO", "VIDEO");
    const NetId syncN = *ConnectorNet(*loaded, "VIDEO", "SYNC_N");

    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    TvInput input(sim, video, syncN);
    while (sim.now() < 60_ms) {
        sim.runFor(1_ms);
        input.update();
        sim.trimRecordings();
    }
    const std::vector<float> reference = input.tv().completedFrame().cells;

    TvConsumer tv(video, syncN);
    ProbeConsumer probe;
    Runner runner(loaded->circuit, {&tv, &probe});
    runner.setSpeed(Speed::Max);
    probe.touch(runner, video);
    runner.pauseAt(20_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(Lamps(probe).ends_with("PULSE"));
    probe.touch(runner, loaded->nets.at("H5"));
    RunTo(runner, 60_ms);
    REQUIRE(tv.pictures().update());
    CHECK(tv.pictures().front().frame.cells == reference);
}

TEST_CASE("probe consumer: touching many Nets in turn", "[runner]") {
    auto loaded = LoadTestPattern();
    ProbeConsumer probe;
    Runner runner(loaded->circuit, {&probe});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    std::size_t touched = 0;
    Time t = 1_ms;
    while (touched <= kMaxWatched) {  // round the Nets again if there are fewer than slots
        for (const auto& [name, net] : loaded->nets) {
            probe.touch(runner, net);
            t += 1_ms;
            RunTo(runner, t);
            ++touched;
        }
    }
    CHECK(touched > kMaxWatched);
    CHECK(runner.status().simTime == t);
}
