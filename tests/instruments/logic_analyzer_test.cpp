#include "instruments/logic_analyzer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/builder.hpp"
using namespace bw;

namespace {

using enum LogicValue;

constexpr Time kPeriod = 1000;
constexpr Time kFar = 10 * static_cast<Time>(kDepth) * kPeriod;
constexpr ChannelMask kCh0 = 1, kCh1 = 2;
const std::size_t kPre = PreTriggerPoints(TriggerPosition::Early);   // 6553
const std::size_t kMid = PreTriggerPoints(TriggerPosition::Middle);  // 32768

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
    REQUIRE(capture.trigger == kPre);
    CHECK(Bit(capture, kPre + 2, 0) == false);
    CHECK(Bit(capture, kPre + 3, 0) == true);
    CHECK(Bit(capture, kPre + 2, 1) == std::nullopt);
    CHECK(Bit(capture, kPre + 3, 1) == true);
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
    CHECK(Bit(capture, kPre + 4, 0) == std::nullopt);
    CHECK(Bit(capture, kPre + 5, 0) == false);
    CHECK(Bit(capture, kPre + 6, 0) == std::nullopt);
    CHECK(Bit(capture, kPre + 7, 0) == true);
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
    CHECK(state.capture().first == kPre);
    CHECK(state.capture().period == kPeriod);
}

TEST_CASE("analyzer: trigger positions", "[instruments]") {
    for (const auto [position, pre] : {std::pair{TriggerPosition::Early, std::size_t{6553}},
                                       std::pair{TriggerPosition::Middle, std::size_t{32768}},
                                       std::pair{TriggerPosition::Late, std::size_t{58982}}}) {
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
    CHECK(capture.first == kMid - 100);
    CHECK(Bit(capture, kMid - 101, 0) == std::nullopt);
    CHECK(Bit(capture, kMid - 100, 0) == false);
    CHECK(Bit(capture, kMid, 0) == true);
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
    const auto loaded = LoadNetlistFile(test::NetlistPath("test_pattern"));
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

TEST_CASE("analyzer: channel 31", "[instruments]") {
    constexpr ChannelMask kCh31 = ChannelMask{1} << 31;
    AnalyzerState state;
    AnalyzerSettings settings = Settings();
    settings.trigger[31] = TriggerCondition::Rise;
    state.arm(settings, {}, 0);
    state.change(0, kCh31, Zero);
    state.change(4500, kCh31, One);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    CHECK(state.capture().triggerTime == 5000);
    CHECK(Bit(state.capture(), kPre - 1, 31) == false);
    CHECK(Bit(state.capture(), kPre, 31) == true);
}

TEST_CASE("analyzer: channel 63, the last of 64", "[instruments]") {
    static_assert(kChannels == 64);
    constexpr ChannelMask kCh63 = ChannelMask{1} << 63;
    AnalyzerState state;
    AnalyzerSettings settings = Settings();
    settings.trigger[63] = TriggerCondition::Fall;
    state.arm(settings, {}, 0);
    state.change(0, kCh63, One);
    state.change(4500, kCh63, Zero);
    state.advanceTo(kFar);
    REQUIRE(state.phase() == AnalyzerPhase::Captured);
    CHECK(state.capture().triggerTime == 5000);
    CHECK(Bit(state.capture(), kPre - 1, 63) == true);
    CHECK(Bit(state.capture(), kPre, 63) == false);
}

TEST_CASE("analyzer: mask trigger matches the per-Channel rule", "[instruments]") {
    // The P3c evaluator, Channel by Channel, as the reference.
    const auto reference = [](const AnalyzerSettings& settings, const ChannelBits* previous,
                              ChannelBits now) {
        for (std::size_t c = 0; c < kChannels; ++c) {
            const ChannelMask bit = ChannelMask{1} << c;
            const bool known = (now.unknown & bit) == 0;
            const bool high = (now.high & bit) != 0;
            const bool wasKnown = previous && (previous->unknown & bit) == 0;
            const bool wasHigh = previous && (previous->high & bit) != 0;
            bool ok = true;
            switch (settings.trigger[c]) {
            case TriggerCondition::Any:
                break;
            case TriggerCondition::Low:
                ok = known && !high;
                break;
            case TriggerCondition::High:
                ok = known && high;
                break;
            case TriggerCondition::Rise:
                ok = known && wasKnown && !wasHigh && high;
                break;
            case TriggerCondition::Fall:
                ok = known && wasKnown && wasHigh && !high;
                break;
            }
            if (!ok) {
                return false;
            }
        }
        return true;
    };
    std::mt19937_64 rng(5);
    for (int run = 0; run < 200; ++run) {
        // Few Channels with a condition, on a few lines, so Triggers do happen.
        AnalyzerSettings settings = Settings();
        for (std::size_t c = 0; c < 3; ++c) {
            settings.trigger[rng() % 6] = static_cast<TriggerCondition>(rng() % 5);
        }
        AnalyzerState state;
        state.arm(settings, {}, 0);
        ChannelBits now;
        std::optional<ChannelBits> previous;
        std::optional<Time> expected;
        for (Time t = 0; t < 200 * kPeriod; t += kPeriod) {
            const auto channels = static_cast<ChannelMask>(rng() & 0x3f);
            const auto value = static_cast<LogicValue>(rng() % 3);  // Zero, One or Z (unknown)
            state.change(t, channels, value);
            switch (value) {
            case LogicValue::Zero:
                now.high &= ~channels;
                now.unknown &= ~channels;
                break;
            case LogicValue::One:
                now.high |= channels;
                now.unknown &= ~channels;
                break;
            default:
                now.unknown |= channels;
                break;
            }
            if (!expected && reference(settings, previous ? &*previous : nullptr, now)) {
                expected = t;
            }
            previous = now;
        }
        state.advanceTo(200 * kPeriod - 1);
        INFO(run);
        if (expected) {
            REQUIRE(state.phase() != AnalyzerPhase::Armed);
            state.advanceTo(kFar);
            CHECK(state.capture().triggerTime == *expected);
        } else {
            CHECK(state.phase() == AnalyzerPhase::Armed);
        }
    }
}

TEST_CASE("analyzer: ParseBusPattern", "[instruments]") {
    using enum TriggerCondition;
    const auto p8000 = ParseBusPattern("8000", 16);
    REQUIRE(p8000.has_value());
    REQUIRE(p8000->size() == 16);
    CHECK((*p8000)[15] == High);
    for (std::size_t b = 0; b < 15; ++b) {
        CHECK((*p8000)[b] == Low);
    }
    const auto e0xx = ParseBusPattern("E0xX", 16);
    REQUIRE(e0xx.has_value());
    for (std::size_t b = 0; b < 8; ++b) {
        CHECK((*e0xx)[b] == Any);
    }
    CHECK((*e0xx)[8] == Low);
    CHECK((*e0xx)[13] == High);
    CHECK((*e0xx)[15] == High);
    const auto short5 = ParseBusPattern("5", 8);  // padded: 05
    REQUIRE(short5.has_value());
    CHECK(*short5 == std::vector{High, Low, High, Low, Low, Low, Low, Low});
    CHECK(ParseBusPattern("", 4) == std::vector{Any, Any, Any, Any});
    CHECK(ParseBusPattern("3", 2) == std::vector{High, High});
    CHECK(ParseBusPattern("4", 2) == std::nullopt);  // a 1 above the width
    CHECK(ParseBusPattern("123", 8) == std::nullopt);
    CHECK(ParseBusPattern("G0", 8) == std::nullopt);
    CHECK(ParseBusPattern(" 0", 8) == std::nullopt);
}

TEST_CASE("analyzer: FormatBusPattern", "[instruments]") {
    using enum TriggerCondition;
    for (const char* text : {"8000", "E0XX", "XXXX", "0001"}) {
        const auto conditions = ParseBusPattern(text, 16);
        REQUIRE(conditions.has_value());
        std::string expected = text;
        std::ranges::replace(expected, 'X', 'x');
        CHECK(FormatBusPattern(*conditions) == expected);
    }
    CHECK(FormatBusPattern(std::vector{High, Any, Low, Low}) == "?");
    CHECK(FormatBusPattern(std::vector{Rise, Low, Low, Low, High}) == "1?");
    CHECK(FormatBusPattern(std::vector{High, High, High}) == "7");
}

TEST_CASE("analyzer: StemOf", "[instruments]") {
    CHECK(StemOf("A0") == NameStem{"A#", 0});
    CHECK(StemOf("A15") == NameStem{"A#", 15});
    CHECK(StemOf("1Q") == NameStem{"#Q", 1});
    CHECK(StemOf("8Q") == NameStem{"#Q", 8});
    CHECK(StemOf("1A1") == NameStem{"1A#", 1});
    CHECK(StemOf("2A1") == NameStem{"2A#", 1});
    CHECK(StemOf("I/O1") == NameStem{"I/O#", 1});
    CHECK(StemOf("Y7_N") == NameStem{"Y#_N", 7});
    CHECK(StemOf("RW") == std::nullopt);
}
