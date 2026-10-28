#include "sim/recorder.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("recorder: start stores initial sample", "[sim]") {
    Recorder recorder;
    const NetId net{3};
    CHECK_FALSE(recorder.isRecording(net));
    recorder.start(net, 10, LogicValue::One);
    CHECK(recorder.isRecording(net));
    const auto samples = recorder.samples(net);
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].time == 10);
    CHECK(samples[0].value == LogicValue::One);
}

TEST_CASE("recorder: start twice is a no-op", "[sim]") {
    Recorder recorder;
    const NetId net{0};
    recorder.start(net, 10, LogicValue::One);
    recorder.start(net, 20, LogicValue::Zero);
    CHECK(recorder.samples(net).size() == 1);
    CHECK(recorder.nets().size() == 1);
}

TEST_CASE("recorder: add appends", "[sim]") {
    Recorder recorder;
    const NetId net{1};
    recorder.start(net, 0, LogicValue::Zero);
    recorder.add(net, 5, LogicValue::One);
    recorder.add(net, 7, LogicValue::X);
    const auto samples = recorder.samples(net);
    REQUIRE(samples.size() == 3);
    CHECK(samples[1].time == 5);
    CHECK(samples[1].value == LogicValue::One);
    CHECK(samples[2].time == 7);
    CHECK(samples[2].value == LogicValue::X);
}

TEST_CASE("recorder: nets in start order", "[sim]") {
    Recorder recorder;
    recorder.start(NetId{5}, 0, LogicValue::Zero);
    recorder.start(NetId{1}, 0, LogicValue::Zero);
    recorder.start(NetId{3}, 0, LogicValue::Zero);
    const auto nets = recorder.nets();
    REQUIRE(nets.size() == 3);
    CHECK(nets[0] == NetId{5});
    CHECK(nets[1] == NetId{1});
    CHECK(nets[2] == NetId{3});
}

TEST_CASE("recorder: trim keeps the last sample", "[sim]") {
    Recorder recorder;
    const NetId net{1};
    recorder.start(net, 0, LogicValue::Zero);
    recorder.add(net, 5, LogicValue::One);
    recorder.add(net, 7, LogicValue::X);
    recorder.trim(net);
    const auto samples = recorder.samples(net);
    REQUIRE(samples.size() == 1);
    CHECK((samples[0] == Sample{7, LogicValue::X}));
    recorder.trim(net);
    CHECK(recorder.samples(net).size() == 1);
}

TEST_CASE("recorder: trim of an unrecorded net is a no-op", "[sim]") {
    Recorder recorder;
    recorder.trim(NetId{4});
    CHECK_FALSE(recorder.isRecording(NetId{4}));
}

TEST_CASE("recorder: clear forgets everything", "[sim]") {
    Recorder recorder;
    recorder.start(NetId{2}, 0, LogicValue::Zero);
    recorder.clear();
    CHECK_FALSE(recorder.isRecording(NetId{2}));
    CHECK(recorder.nets().empty());
    CHECK(recorder.samples(NetId{2}).empty());
}
