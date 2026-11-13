#include "instruments/oscilloscope.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

ScopeState Scope(const ScopeSettings& settings) {
    ScopeState scope;
    scope.powerOn();
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        scope.clear(c, 0, true);
    }
    scope.configure(settings, 0);
    return scope;
}

}  // namespace

TEST_CASE("scope: sweep around the trigger", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(1, {3_us, LogicValue::One});
    scope.change(0, {5_us, LogicValue::One});
    scope.change(0, {5500_ns, LogicValue::Zero});
    scope.change(1, {6_us, LogicValue::Zero});
    scope.change(1, {20_us, LogicValue::One});
    scope.advanceTo(30_us);
    const Sweep& sweep = scope.sweep();
    CHECK(sweep.serial == 1);
    CHECK(sweep.kind == SweepKind::Triggered);
    CHECK(scope.status() == SweepKind::Triggered);
    CHECK(sweep.start == 4_us);
    CHECK(sweep.timebase == 1_us);
    CHECK(sweep.channels[0] == std::vector<Sample>{{4_us, LogicValue::Zero},
                                                   {5_us, LogicValue::One},
                                                   {5500_ns, LogicValue::Zero}});
    CHECK(sweep.channels[1] ==
          std::vector<Sample>{{4_us, LogicValue::One}, {6_us, LogicValue::Zero}});
}

TEST_CASE("scope: auto free-runs on a constant net", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.change(0, {0, LogicValue::One});
    scope.advanceTo(60_ms);
    CHECK(scope.sweep().serial == 1);
    CHECK(scope.sweep().kind == SweepKind::Auto);
    CHECK(scope.status() == SweepKind::Auto);
    CHECK(scope.sweep().start == 50_ms - 1_us);
    CHECK(scope.sweep().channels[0] == std::vector<Sample>{{50_ms - 1_us, LogicValue::One}});
}

TEST_CASE("scope: normal waits", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .mode = ScopeMode::Normal});
    scope.change(0, {0, LogicValue::One});
    scope.advanceTo(60_ms);
    CHECK(scope.sweep().serial == 0);
    CHECK(scope.status() == SweepKind::None);
}

TEST_CASE("scope: fall trigger on ch2", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .source = 1, .edge = ScopeEdge::Fall});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(1, {0, LogicValue::Zero});
    scope.change(0, {2_us, LogicValue::One});
    scope.change(0, {3_us, LogicValue::Zero});
    scope.change(1, {4_us, LogicValue::One});
    scope.change(1, {7_us, LogicValue::Zero});
    scope.advanceTo(30_us);
    CHECK(scope.sweep().serial == 1);
    CHECK(scope.sweep().start == 6_us);
}

TEST_CASE("scope: X before power-on", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(0, {500_ns, LogicValue::One});
    scope.advanceTo(30_us);
    REQUIRE(scope.sweep().serial == 1);
    CHECK(scope.sweep().start == -500'000);
    CHECK(scope.sweep().channels[0] == std::vector<Sample>{{-500'000, LogicValue::X},
                                                           {0, LogicValue::Zero},
                                                           {500_ns, LogicValue::One}});
}

TEST_CASE("scope: only the last sweep per advance", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.change(0, {0, LogicValue::Zero});
    for (const Time t : {5_us, 25_us, 45_us}) {
        scope.change(0, {t, LogicValue::One});
        scope.change(0, {t + 5_us, LogicValue::Zero});
    }
    scope.advanceTo(100_us);
    CHECK(scope.sweep().serial == 1);
    CHECK(scope.sweep().start == 44_us);
}

TEST_CASE("scope: configure drops a sweep in progress", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(0, {5_us, LogicValue::One});
    scope.configure({.timebase = 1_us}, 7_us);
    scope.advanceTo(30_us);
    CHECK(scope.sweep().serial == 0);
    CHECK(scope.status() == SweepKind::None);
}

TEST_CASE("scope: unhooked channel stays empty", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.clear(1, 0, false);
    scope.change(0, {0, LogicValue::Zero});
    scope.change(0, {5_us, LogicValue::One});
    scope.advanceTo(30_us);
    REQUIRE(scope.sweep().serial == 1);
    CHECK(scope.sweep().channels[1].empty());
}

TEST_CASE("scope: seed gives the current value", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us});
    scope.advanceTo(1_ms);
    scope.seed(1, {0, LogicValue::One});
    scope.change(0, {2_ms, LogicValue::Zero});
    scope.change(0, {3_ms, LogicValue::One});
    scope.advanceTo(4_ms);
    REQUIRE(scope.sweep().serial == 1);
    CHECK(scope.sweep().channels[1] == std::vector<Sample>{{3_ms - 1_us, LogicValue::One}});
}

TEST_CASE("scope: timebase index", "[instruments]") {
    CHECK(TimebaseIndex(5_ns) == 0u);
    CHECK(TimebaseIndex(20_ns) == 2u);
    CHECK(TimebaseIndex(10_ms) == 19u);
    CHECK_FALSE(TimebaseIndex(3_ns).has_value());
}

TEST_CASE("scope: rise trigger on ch4", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .source = 3, .mode = ScopeMode::Normal});
    scope.change(2, {0, LogicValue::One});
    scope.change(3, {0, LogicValue::Zero});
    scope.change(0, {2_us, LogicValue::One});  // ch1 rises: not the source
    scope.change(3, {5_us, LogicValue::One});
    scope.advanceTo(30_us);
    REQUIRE(scope.sweep().serial == 1);
    CHECK(scope.sweep().start == 4_us);
    CHECK(scope.sweep().channels[2] == std::vector<Sample>{{4_us, LogicValue::One}});
    CHECK(scope.sweep().channels[3] ==
          std::vector<Sample>{{4_us, LogicValue::Zero}, {5_us, LogicValue::One}});
}

TEST_CASE("scope: delay moves the sweep after the trigger", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .mode = ScopeMode::Normal, .delay = 5_us});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(1, {0, LogicValue::Zero});
    scope.change(0, {5_us, LogicValue::One});   // the Trigger
    scope.change(0, {7_us, LogicValue::Zero});  // a rise during the delay: ignored
    scope.change(0, {8_us, LogicValue::One});
    scope.change(1, {9_us, LogicValue::One});    // before the screen's left edge
    scope.change(1, {12_us, LogicValue::Zero});  // on screen
    scope.advanceTo(30_us);
    const Sweep& sweep = scope.sweep();
    REQUIRE(sweep.serial == 1);
    CHECK(sweep.delay == 5_us);
    CHECK(sweep.start == 9_us);  // 5 + 5 - 1 us
    CHECK(sweep.channels[0] == std::vector<Sample>{{9_us, LogicValue::One}});
    CHECK(sweep.channels[1] ==
          std::vector<Sample>{{9_us, LogicValue::One}, {12_us, LogicValue::Zero}});
}

TEST_CASE("scope: delay clamped to 1000 divisions", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .mode = ScopeMode::Normal, .delay = 5_ms});
    scope.change(0, {0, LogicValue::Zero});
    scope.change(0, {1_us, LogicValue::One});
    scope.advanceTo(2_ms);
    REQUIRE(scope.sweep().serial == 1);
    CHECK(scope.sweep().delay == 1_ms);
    CHECK(scope.sweep().start == 1_ms);  // 1 us + 1 ms - 1 us
}

TEST_CASE("scope: auto with a delay", "[instruments]") {
    ScopeState scope = Scope({.timebase = 1_us, .delay = 3_us});
    scope.change(0, {0, LogicValue::One});
    scope.advanceTo(60_ms);
    CHECK(scope.sweep().kind == SweepKind::Auto);
    CHECK(scope.sweep().start == 50_ms + 3_us - 1_us);
}
