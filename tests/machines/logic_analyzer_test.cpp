#include "instruments/logic_analyzer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// Capture at 1 MHz, all Any, of the given pads in the built `puzzle`.
Capture CaptureOf(const std::string& puzzle,
                  std::initializer_list<std::pair<const char*, const char*>> pins) {
    const auto machine = LoadPuzzleFile(test::PuzzlePath(puzzle));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    std::array<std::optional<NetId>, kChannels> channels;
    std::size_t c = 0;
    for (const auto& [reference, pin] : pins) {
        channels[c] = PinNet(**built, reference, pin);
        REQUIRE(channels[c].has_value());
        ++c;
    }
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    LogicAnalyzer analyzer(sim, channels);
    analyzer.arm({.period = 1_us});
    while (analyzer.state().phase() != AnalyzerPhase::Captured && sim.now() < 100_ms) {
        sim.runUntil(sim.now() + 1_ms);
        analyzer.update();
    }
    REQUIRE(analyzer.state().phase() == AnalyzerPhase::Captured);
    return analyzer.state().capture();
}

std::size_t Changes(const Capture& capture, std::size_t channel) {
    std::size_t changes = 0;
    for (std::size_t p = capture.first + 1; p < capture.points.size(); ++p) {
        const auto bit = [&](std::size_t i) {
            return (capture.points[i].high >> channel & 1) |
                   (capture.points[i].unknown >> channel & 1) << 1;
        };
        changes += bit(p) != bit(p - 1) ? 1 : 0;
    }
    return changes;
}

}  // namespace

TEST_CASE("analyzer: crack stops H0", "[machines]") {
    const Capture capture = CaptureOf("broken_trace", {{"U1", "14"}, {"U13", "6"}});
    CHECK(Changes(capture, 0) > 0);
    CHECK(Changes(capture, 1) == 0);
}

TEST_CASE("analyzer: stuck gate", "[machines]") {
    const Capture capture = CaptureOf("stuck_gate", {{"U12", "11"}});
    for (std::size_t p = capture.first; p < kDepth; ++p) {
        CHECK((capture.points[p].unknown & 1) == 0);
        CHECK((capture.points[p].high & 1) == 1);
    }
}

namespace {

// 110 LED computer at 10 MHz: Channels 0-15 U1 A0-A15, 16-23 U1 D0-D7, 24 RW, 25 PHI2; the
// address Trigger `pattern` with RW at `rw`.
Capture CaptureBus(const char* pattern, TriggerCondition rw) {
    auto machine = LoadNetlistMachine(test::NetlistPath("led_computer"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    std::array<std::optional<NetId>, kChannels> channels;
    for (std::size_t b = 0; b < 16; ++b) {
        channels[b] = PinNet(**built, "U1", "A" + std::to_string(b));
    }
    for (std::size_t b = 0; b < 8; ++b) {
        channels[16 + b] = PinNet(**built, "U1", "D" + std::to_string(b));
    }
    channels[24] = PinNet(**built, "U1", "RW");
    channels[25] = PinNet(**built, "U1", "PHI2");
    AnalyzerSettings settings{.period = 100_ns};
    const auto address = ParseBusPattern(pattern, 16);
    REQUIRE(address.has_value());
    std::ranges::copy(*address, settings.trigger.begin());
    settings.trigger[24] = rw;
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    LogicAnalyzer analyzer(sim, channels);
    analyzer.arm(settings);
    while (analyzer.state().phase() != AnalyzerPhase::Captured && sim.now() < 40_ms) {
        sim.runUntil(sim.now() + 1_ms);
        analyzer.update();
    }
    REQUIRE(analyzer.state().phase() == AnalyzerPhase::Captured);
    return analyzer.state().capture();
}

// Channels first..first+width-1 at `point`, bit 0 first; nullopt if any is X.
std::optional<std::uint32_t> Value(const Capture& capture, std::size_t point, std::size_t first,
                                   std::size_t width) {
    const ChannelBits bits = capture.points.at(point);
    const ChannelMask mask = ((ChannelMask{1} << width) - 1) << first;
    if ((bits.unknown & mask) != 0) {
        return std::nullopt;
    }
    return (bits.high & mask) >> first;
}

// The data bus at the last point of the Trigger's cycle with PHI2 high.
std::optional<std::uint32_t> DataAtEndOfCycle(const Capture& capture) {
    const auto address = Value(capture, capture.trigger, 0, 16);
    REQUIRE(address.has_value());
    std::optional<std::size_t> last;
    for (std::size_t p = capture.trigger;
         p < capture.points.size() && Value(capture, p, 0, 16) == address; ++p) {
        if (Value(capture, p, 25, 1) == 1u) {
            last = p;
        }
    }
    REQUIRE(last.has_value());
    return Value(capture, *last, 16, 8);
}

}  // namespace

TEST_CASE("analyzer: 110 first write to the LEDs", "[machines]") {
    const Capture capture = CaptureBus("8000", TriggerCondition::Low);
    CHECK(capture.triggerTime > 20_ms);  // after the power-on reset
    CHECK(DataAtEndOfCycle(capture) == 0x01u);
}

TEST_CASE("analyzer: 110 reset vector read", "[machines]") {
    const Capture capture = CaptureBus("FFFC", TriggerCondition::High);
    CHECK(DataAtEndOfCycle(capture) == 0x00u);  // $F800, low byte
}
