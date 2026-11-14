#include "runner/runner.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "runner/tv_consumer.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

struct TestPattern {
    std::unique_ptr<LoadedCircuit> loaded;
    NetId video, syncN;
};

TestPattern Load() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    const auto video = ConnectorNet(**result, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(**result, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(syncN.has_value());
    return {std::move(*result), *video, *syncN};
}

// The headless single-thread path.
std::vector<float> Reference(TestPattern& pattern, std::uint64_t seed) {
    Simulator sim(pattern.loaded->circuit);
    sim.powerOn(seed);
    TvInput input(sim, pattern.video, pattern.syncN);
    while (sim.now() < 60_ms) {
        sim.runFor(1_ms);
        input.update();
        sim.trimRecordings();
    }
    REQUIRE(input.tv().completedFrames() >= 1);
    return input.tv().completedFrame().cells;
}

std::vector<float> Latest(TvConsumer& tv) {
    REQUIRE(tv.pictures().update());
    return tv.pictures().front().frame.cells;
}

class SampleLog final : public Consumer {
public:
    explicit SampleLog(NetId net) : net_(net) {}
    std::span<const NetId> nets() const override { return {&net_, 1}; }
    void powerOn() override {}
    void sample(NetId, Sample sample) override { samples.push_back(sample); }
    void seed(NetId, Sample sample) override { seeds.push_back(sample); }
    void advanceTo(Time) override {}
    void powerOff() override {}

    std::vector<Sample> samples, seeds;

private:
    NetId net_;
};

void RunTo(Runner& runner, Time time) {
    runner.pauseAt(time);
    runner.resume();
    runner.waitIdle();
}

}  // namespace

TEST_CASE("runner: frame equals TvInput", "[runner]") {
    const std::uint64_t seed = GENERATE(1, 2);
    TestPattern pattern = Load();
    const auto reference = Reference(pattern, seed);
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(60_ms);
    runner.powerOn(seed);
    runner.waitIdle();
    const RunnerStatus status = runner.status();
    CHECK(status.state == RunState::Paused);
    CHECK(status.simTime == 60_ms);
    CHECK(status.seed == seed);
    CHECK(Latest(tv) == reference);
}

TEST_CASE("runner: tiny ring gives the same frame", "[runner]") {
    TestPattern pattern = Load();
    const auto reference = Reference(pattern, 1);
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv}, 4);
    runner.setSpeed(Speed::Max);
    runner.pauseAt(60_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(Latest(tv) == reference);
    CHECK(runner.status().ringWaitShare > 0.0);
}

TEST_CASE("runner: second consumer on VIDEO", "[runner]") {
    TestPattern pattern = Load();
    Simulator sim(pattern.loaded->circuit);
    sim.powerOn(1);
    sim.record(pattern.video);
    sim.runUntil(60_ms);
    const auto recorded = sim.recorder().samples(pattern.video);
    const std::vector<Sample> reference(recorded.begin(), recorded.end());

    TvConsumer tv(pattern.video, pattern.syncN);
    SampleLog probe(pattern.video);
    Runner runner(pattern.loaded->circuit, {&tv, &probe});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(60_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(probe.samples.size() > 100);
    CHECK(probe.samples == reference);
}

TEST_CASE("runner: pause and resume", "[runner]") {
    TestPattern pattern = Load();
    const auto reference = Reference(pattern, 1);
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(20_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(runner.status().simTime == 20_ms);
    RunTo(runner, 60_ms);
    CHECK(runner.status().simTime == 60_ms);
    CHECK(Latest(tv) == reference);
}

TEST_CASE("runner: power cycle", "[runner]") {
    TestPattern pattern = Load();
    const auto reference = Reference(pattern, 1);
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(30_ms);
    runner.powerOn(2);
    runner.waitIdle();
    runner.pauseAt(60_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(runner.status().seed == 1);
    CHECK(Latest(tv) == reference);
}

TEST_CASE("runner: power off shows black", "[runner]") {
    TestPattern pattern = Load();
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(60_ms);
    runner.powerOn(1);
    runner.waitIdle();
    runner.powerOff();
    runner.waitIdle();
    CHECK(runner.status().state == RunState::Off);
    REQUIRE(tv.pictures().update());
    const TvPicture& picture = tv.pictures().front();
    CHECK_FALSE(picture.locked);
    CHECK(std::ranges::all_of(picture.frame.cells, [](float cell) { return cell == 0.0f; }));
}

TEST_CASE("runner: watch and unwatch", "[runner]") {
    TestPattern pattern = Load();
    const NetId h5 = pattern.loaded->nets.at("H5");
    Simulator sim(pattern.loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(20_ms);
    sim.record(h5);
    sim.runUntil(40_ms);
    const auto recorded = sim.recorder().samples(h5);
    const std::vector<Sample> reference(recorded.begin(), recorded.end());

    SampleLog probe(h5);
    Runner runner(pattern.loaded->circuit, {&probe});
    runner.unwatch(h5);
    runner.setSpeed(Speed::Max);
    runner.pauseAt(20_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(probe.samples.empty());
    runner.watch(h5);
    RunTo(runner, 40_ms);
    REQUIRE_FALSE(probe.samples.empty());
    CHECK(probe.samples.front().time == 20_ms);
    CHECK(probe.samples == reference);
    runner.unwatch(h5);
    RunTo(runner, 60_ms);
    CHECK(probe.samples == reference);
}

TEST_CASE("runner: watches are counted", "[runner]") {
    TestPattern pattern = Load();
    const auto reference = Reference(pattern, 1);
    TvConsumer tv(pattern.video, pattern.syncN);
    Runner runner(pattern.loaded->circuit, {&tv});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(20_ms);
    runner.powerOn(1);
    runner.waitIdle();
    runner.watch(pattern.video);
    runner.unwatch(pattern.video);
    RunTo(runner, 60_ms);
    CHECK(Latest(tv) == reference);
}

TEST_CASE("runner: rewatch seeds the last value", "[runner]") {
    TestPattern pattern = Load();
    const auto clear = PinNet(*pattern.loaded, "U1", "1");
    REQUIRE(clear.has_value());
    SampleLog log(*clear);
    Runner runner(pattern.loaded->circuit, {&log});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(log.seeds.empty());
    runner.watch(*clear);
    runner.waitIdle();
    REQUIRE(log.seeds.size() == 1);
    CHECK(log.seeds[0].value == LogicValue::One);
}

TEST_CASE("runner: wake applies a button press at the next slice", "[runner]") {
    auto result = LoadNetlist("part R1 R value=4k7\npart SW1 BUTTON\npart U1 74LS04\n"
                              "net VCC R1.1\nnet BTN_N R1.2 SW1.1 U1.1A\nnet GND SW1.2\n",
                              "t.bwn");
    REQUIRE(result.has_value());
    const NetId signal = (*result)->nets.at("BTN_N");
    Switch* button = SwitchOf(**result, "SW1");
    REQUIRE(button != nullptr);
    SampleLog log(signal);
    Runner runner((*result)->circuit, {&log});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(2_ms);
    runner.powerOn(1);
    runner.waitIdle();
    REQUIRE(runner.status().simTime == 2_ms);
    button->setPressed(true);
    runner.wake(button->id());
    RunTo(runner, 4_ms);
    REQUIRE_FALSE(log.samples.empty());
    const Sample& last = log.samples.back();
    CHECK(last.value == LogicValue::Zero);
    CHECK(last.time == 2_ms + kPressLatency);
}

namespace {

class ManyNets final : public Consumer {
public:
    explicit ManyNets(std::vector<NetId> nets) : nets_(std::move(nets)) {}
    std::span<const NetId> nets() const override { return nets_; }
    void powerOn() override {}
    void sample(NetId net, Sample) override { ++counts[net]; }
    void seed(NetId net, Sample) override { ++counts[net]; }
    void advanceTo(Time) override {}
    void powerOff() override {}

    std::map<NetId, std::size_t> counts;

private:
    std::vector<NetId> nets_;
};

}  // namespace

TEST_CASE("runner: 96 watched Nets", "[runner]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "tennis.bwn");
    REQUIRE(loaded.has_value());
    std::vector<NetId> nets;
    for (const auto& [name, net] : (*loaded)->nets) {
        if (nets.size() < kMaxWatched && std::ranges::find(nets, net) == nets.end()) {
            nets.push_back(net);
        }
    }
    REQUIRE(nets.size() == kMaxWatched);
    ManyNets many(nets);
    Runner runner((*loaded)->circuit, {&many});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(2_ms);
    runner.powerOn(1);
    runner.waitIdle();
    for (const NetId net : nets) {
        INFO(ToIndex(net));
        CHECK(many.counts[net] >= 1);
    }
}
