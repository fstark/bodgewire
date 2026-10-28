#include "runner/dispatcher.hpp"

#include <array>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "runner/tv_consumer.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using Log = std::vector<std::string>;

class RecordingConsumer final : public Consumer {
public:
    explicit RecordingConsumer(std::vector<NetId> nets) : nets_(std::move(nets)) {}
    std::span<const NetId> nets() const override { return nets_; }
    void powerOn() override { log.push_back("on"); }
    void sample(NetId net, Sample sample) override {
        log.push_back(std::format("{} {} {}", ToIndex(net), sample.time, ToChar(sample.value)));
    }
    void advanceTo(Time time) override { log.push_back(std::format("adv {}", time)); }
    void powerOff() override { log.push_back("off"); }

    Log log;

private:
    std::vector<NetId> nets_;
};

Marker Watch(std::uint8_t slot, NetId net) { return {MarkerKind::Watch, slot, net, 0}; }
Marker SliceEnd(Time time) { return {MarkerKind::SliceEnd, 0, NetId{}, time}; }

}  // namespace

TEST_CASE("dispatcher: merges by time then slot", "[runner]") {
    RecordingConsumer consumer({NetId{7}, NetId{3}});
    Dispatcher dispatcher({&consumer});
    dispatcher.apply(Watch(0, NetId{7}));
    dispatcher.apply(Watch(1, NetId{3}));
    dispatcher.stage(1, {0, LogicValue::Zero});
    dispatcher.stage(1, {5, LogicValue::One});
    dispatcher.stage(0, {0, LogicValue::One});
    dispatcher.stage(0, {5, LogicValue::Zero});
    dispatcher.stage(0, {9, LogicValue::One});
    dispatcher.apply(SliceEnd(10));
    CHECK(consumer.log == Log{"7 0 1", "3 0 0", "7 5 0", "3 5 1", "7 9 1", "adv 10"});
}

TEST_CASE("dispatcher: keeps Samples past the slice end", "[runner]") {
    RecordingConsumer consumer({NetId{2}});
    Dispatcher dispatcher({&consumer});
    dispatcher.apply(Watch(0, NetId{2}));
    dispatcher.stage(0, {4, LogicValue::One});
    dispatcher.stage(0, {12, LogicValue::Zero});
    dispatcher.apply(SliceEnd(10));
    dispatcher.apply(SliceEnd(20));
    CHECK(consumer.log == Log{"2 4 1", "adv 10", "2 12 0", "adv 20"});
}

TEST_CASE("dispatcher: two consumers on one Net", "[runner]") {
    RecordingConsumer both({NetId{1}, NetId{2}});
    RecordingConsumer one({NetId{1}});
    Dispatcher dispatcher({&both, &one});
    dispatcher.apply(Watch(0, NetId{1}));
    dispatcher.apply(Watch(1, NetId{2}));
    dispatcher.stage(0, {3, LogicValue::One});
    dispatcher.stage(1, {4, LogicValue::Zero});
    dispatcher.apply(SliceEnd(10));
    CHECK(both.log == Log{"1 3 1", "2 4 0", "adv 10"});
    CHECK(one.log == Log{"1 3 1", "adv 10"});
}

TEST_CASE("dispatcher: power on keeps Samples of the new run", "[runner]") {
    RecordingConsumer consumer({NetId{1}});
    Dispatcher dispatcher({&consumer});
    dispatcher.apply(Watch(0, NetId{1}));
    dispatcher.stage(0, {0, LogicValue::Zero});
    dispatcher.apply({MarkerKind::PowerOn});
    dispatcher.apply(SliceEnd(10));
    dispatcher.apply({MarkerKind::PowerOff});
    CHECK(consumer.log == Log{"on", "1 0 0", "adv 10", "off"});
}

TEST_CASE("dispatcher: unwatch", "[runner]") {
    RecordingConsumer consumer({NetId{1}});
    Dispatcher dispatcher({&consumer});
    dispatcher.apply(Watch(0, NetId{1}));
    CHECK(dispatcher.isActive(0));
    dispatcher.stage(0, {3, LogicValue::One});
    dispatcher.apply({MarkerKind::Unwatch, 0});
    CHECK_FALSE(dispatcher.isActive(0));
    dispatcher.apply(SliceEnd(10));
    CHECK(consumer.log == Log{"adv 10"});
}

TEST_CASE("dispatcher: TV frame equals TvInput", "[runner]") {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    const auto video = ConnectorNet(loaded, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(loaded, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(syncN.has_value());

    Simulator reference(loaded.circuit);
    reference.powerOn(1);
    TvInput input(reference, *video, *syncN);
    while (reference.now() < 60_ms) {
        reference.runFor(1_ms);
        input.update();
        reference.trimRecordings();
    }

    TvConsumer tv(*video, *syncN);
    Dispatcher dispatcher({&tv});
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    const std::array<NetId, 2> nets{*video, *syncN};
    for (std::uint8_t slot = 0; slot < 2; ++slot) {
        sim.record(nets[slot]);
        dispatcher.apply(Watch(slot, nets[slot]));
    }
    dispatcher.apply({MarkerKind::PowerOn});
    Time fedUntil = -1;
    while (sim.now() < 60_ms) {
        sim.runFor(1_ms);
        for (std::uint8_t slot = 0; slot < 2; ++slot) {
            for (const Sample sample : sim.recorder().samples(nets[slot])) {
                if (sample.time > fedUntil) {
                    dispatcher.stage(slot, sample);
                }
            }
        }
        fedUntil = sim.now();
        dispatcher.apply(SliceEnd(sim.now()));
        sim.trimRecordings();
    }
    REQUIRE(input.tv().completedFrames() >= 1);
    REQUIRE(tv.pictures().update());
    CHECK(tv.pictures().front().frame.cells == input.tv().completedFrame().cells);
    CHECK(tv.pictures().front().locked == input.tv().isLocked());
}
