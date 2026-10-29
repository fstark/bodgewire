#include "instruments/logic_analyzer.hpp"

#include <filesystem>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
using namespace bw;

namespace {

using enum LogicValue;

constexpr Time kPeriod = 1000;
constexpr Time kFar = 10 * static_cast<Time>(kDepth) * kPeriod;
constexpr std::uint8_t kCh0 = 1, kCh1 = 2;

// nullopt = X.
std::optional<bool> Bit(const Capture& capture, std::size_t point, std::size_t channel) {
    const ChannelBits bits = capture.points.at(point);
    if ((bits.unknown >> channel & 1) != 0) {
        return std::nullopt;
    }
    return (bits.high >> channel & 1) != 0;
}

AnalyzerSettings Settings(TriggerPosition position = TriggerPosition::Early) {
    return {.period = kPeriod, .position = position};
}

}  // namespace

TEST_CASE("analyzer: point reads last change at or before it", "[instruments]") {
    AnalyzerState state;
    state.arm(Settings(), {}, 0);
    state.change(0, kCh0, Zero);
    state.change(2500, kCh0, One);
    state.change(3000, kCh1, One);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    const Capture& capture = state.capture();
    REQUIRE(capture.trigger == 409);
    CHECK(Bit(capture, 409 + 2, 0) == false);
    CHECK(Bit(capture, 409 + 3, 0) == true);
    CHECK(Bit(capture, 409 + 2, 1) == std::nullopt);
    CHECK(Bit(capture, 409 + 3, 1) == true);
}

TEST_CASE("analyzer: short pulse is missed", "[instruments]") {
    AnalyzerState state;
    AnalyzerSettings settings = Settings();
    settings.trigger[0] = TriggerCondition::Rise;
    state.arm(settings, {}, 0);
    state.change(0, kCh0, Zero);
    state.change(3100, kCh0, One);
    state.change(3900, kCh0, Zero);
    state.advanceTo(10'000);
    CHECK(state.phase() == AnalyzerPhase::Armed);
    state.change(10'500, kCh0, One);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    const Capture& capture = state.capture();
    CHECK(capture.triggerTime == 11'000);
    for (std::size_t p = capture.first; p < capture.trigger; ++p) {
        CHECK(Bit(capture, p, 0) == false);
    }
}

TEST_CASE("analyzer: X shows as X", "[instruments]") {
    AnalyzerState state;
    state.arm(Settings(), {}, 0);
    state.change(5000, kCh0, Zero);
    state.change(6000, kCh0, X);
    state.change(7000, kCh0, One);
    state.advanceTo(kFar);
    const Capture& capture = state.capture();
    CHECK(Bit(capture, 409 + 4, 0) == std::nullopt);
    CHECK(Bit(capture, 409 + 5, 0) == false);
    CHECK(Bit(capture, 409 + 6, 0) == std::nullopt);
    CHECK(Bit(capture, 409 + 7, 0) == true);
}

TEST_CASE("analyzer: rise, fall, levels ANDed", "[instruments]") {
    SECTION("rise and high") {
        AnalyzerState state;
        AnalyzerSettings settings = Settings();
        settings.trigger[0] = TriggerCondition::Rise;
        settings.trigger[1] = TriggerCondition::High;
        state.arm(settings, {}, 0);
        state.change(0, kCh0 | kCh1, Zero);
        state.change(2000, kCh0, One);  // ch1 low: no
        state.change(4000, kCh0, Zero);
        state.change(5000, kCh1, One);
        state.change(7000, kCh0, One);
        state.advanceTo(kFar);
        REQUIRE(state.phase() == AnalyzerPhase::Captured);
        CHECK(state.capture().triggerTime == 7000);
    }
    SECTION("fall and low") {
        AnalyzerState state;
        AnalyzerSettings settings = Settings();
        settings.trigger[0] = TriggerCondition::Fall;
        settings.trigger[1] = TriggerCondition::Low;
        state.arm(settings, {}, 0);
        state.change(0, kCh0 | kCh1, One);
        state.change(2000, kCh0, Zero);  // ch1 high: no
        state.change(3000, kCh0, One);
        state.change(4000, kCh1, Zero);
        state.change(6000, kCh0, Zero);
        state.advanceTo(kFar);
        REQUIRE(state.phase() == AnalyzerPhase::Captured);
        CHECK(state.capture().triggerTime == 6000);
    }
    SECTION("no edge from X") {
        AnalyzerState state;
        AnalyzerSettings settings = Settings();
        settings.trigger[0] = TriggerCondition::Rise;
        state.arm(settings, {}, 0);
        state.change(2000, kCh0, One);
        state.advanceTo(kFar);
        CHECK(state.phase() == AnalyzerPhase::Armed);
    }
}

TEST_CASE("analyzer: all any triggers at once", "[instruments]") {
    AnalyzerState state;
    state.arm(Settings(), {}, 1500);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    CHECK(state.capture().triggerTime == 2000);
    CHECK(state.capture().first == 409);
    CHECK(state.capture().period == kPeriod);
}

TEST_CASE("analyzer: trigger positions", "[instruments]") {
    for (const auto [position, pre] : {std::pair{TriggerPosition::Early, std::size_t{409}},
                                       std::pair{TriggerPosition::Middle, std::size_t{2048}},
                                       std::pair{TriggerPosition::Late, std::size_t{3686}}}) {
        AnalyzerState state;
        state.arm(Settings(position), {}, 0);
        state.advanceTo(kFar);
        REQUIRE(state.phase() == AnalyzerPhase::Captured);
        CHECK(state.capture().trigger == pre);
        CHECK(state.capture().points.size() == kDepth);
    }
}

TEST_CASE("analyzer: early trigger leaves start empty", "[instruments]") {
    AnalyzerState state;
    AnalyzerSettings settings = Settings(TriggerPosition::Middle);
    settings.trigger[0] = TriggerCondition::Rise;
    state.arm(settings, {}, 0);
    state.change(0, kCh0, Zero);
    state.change(99'500, kCh0, One);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    const Capture& capture = state.capture();
    CHECK(capture.triggerTime == 100'000);
    CHECK(capture.first == 1948);
    CHECK(Bit(capture, 1947, 0) == std::nullopt);
    CHECK(Bit(capture, 1948, 0) == false);
    CHECK(Bit(capture, 2048, 0) == true);
}

TEST_CASE("analyzer: power-off drops filling", "[instruments]") {
    AnalyzerState state;
    state.arm(Settings(), {}, 0);
    state.change(0, kCh0, Zero);
    state.advanceTo(100 * kPeriod);
    REQUIRE(state.phase() == AnalyzerPhase::Filling);
    state.powerOff();
    CHECK(state.phase() == AnalyzerPhase::Armed);
    state.powerOn();
    state.change(0, kCh0, One);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    CHECK(state.capture().serial == 1);
    CHECK(state.capture().triggerTime == 0);
    CHECK(Bit(state.capture(), state.capture().trigger, 0) == true);
}

TEST_CASE("analyzer: disarm keeps capture", "[instruments]") {
    AnalyzerState state;
    state.arm(Settings(), {"U1.1 A"}, 0);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    state.disarm();
    CHECK(state.phase() == AnalyzerPhase::Captured);
    state.arm(Settings(), {}, kFar);
    state.disarm();
    CHECK(state.phase() == AnalyzerPhase::Idle);
    CHECK(state.capture().serial == 1);
    CHECK(state.capture().labels[0] == "U1.1 A");
}

TEST_CASE("analyzer: ParseTrigger", "[instruments]") {
    CHECK(ParseTrigger("0") == TriggerCondition::Low);
    CHECK(ParseTrigger("1") == TriggerCondition::High);
    CHECK(ParseTrigger("r") == TriggerCondition::Rise);
    CHECK(ParseTrigger("f") == TriggerCondition::Fall);
    CHECK(ParseTrigger("x") == std::nullopt);
    CHECK(ParseTrigger("") == std::nullopt);
}

TEST_CASE("analyzer: LINE_END_N every 64 points", "[instruments]") {
    using namespace bw::literals;
    const auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(loaded.has_value());
    const auto lineEnd = PinNet(**loaded, "U6", "6");
    const auto load = PinNet(**loaded, "U2", "9");
    REQUIRE(lineEnd.has_value());
    REQUIRE(load.has_value());
    Simulator sim((*loaded)->circuit);
    sim.powerOn(1);
    LogicAnalyzer analyzer(sim, {lineEnd, load});
    AnalyzerSettings settings{.period = 1_us};
    settings.trigger[0] = TriggerCondition::Fall;
    analyzer.arm(settings);
    while (analyzer.state().phase() != AnalyzerPhase::Captured && sim.now() < 100_ms) {
        sim.runUntil(sim.now() + 1_ms);
        analyzer.update();
    }
    REQUIRE(analyzer.state().phase() == AnalyzerPhase::Captured);
    const Capture& capture = analyzer.state().capture();
    CHECK(Bit(capture, capture.trigger, 0) == false);
    std::size_t next = capture.trigger + 1;
    while (next < kDepth &&
           !(Bit(capture, next - 1, 0) == true && Bit(capture, next, 0) == false)) {
        ++next;
    }
    CHECK(next == capture.trigger + 64);
    for (std::size_t p = capture.first; p < kDepth; ++p) {
        CHECK(Bit(capture, p, 1) == Bit(capture, p, 0));
    }
}
