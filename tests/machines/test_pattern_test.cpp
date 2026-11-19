#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"
#include "tv_window.hpp"
#include "view/tv_controls.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr int kPixelsPerLine = 64, kLinesPerFrame = 312;
constexpr int kPixelsPerFrame = kPixelsPerLine * kLinesPerFrame;  // 19'968
constexpr Time kPixel = 1_us;
constexpr Time kFrame = kPixelsPerFrame * kPixel;

struct Outputs {
    bool syncN;
    bool video;
};

// S1/S2 decode of counter state (h, v), before the output register.
constexpr Outputs Decode(int h, int v) {
    const bool hsync = h >= 52 && h <= 55;
    const bool vsync = v >= 288 && v <= 291;  // right after the grid (P6n)
    const bool active = h < 48 && v < 288;
    return {hsync == vsync, active && (h % 8 == 0 || v % 16 == 0)};
}

// Registered output while the counter shows index n (n = v * 64 + h): Decode of n - 1.
constexpr Outputs Expected(int n) {
    const int m = ((n - 1) % kPixelsPerFrame + kPixelsPerFrame) % kPixelsPerFrame;
    return Decode(m % kPixelsPerLine, m / kPixelsPerLine);
}

LogicValue ValueAt(std::span<const Sample> samples, Time t) {
    const auto after =
        std::upper_bound(samples.begin(), samples.end(), t,
                         [](Time time, const Sample& sample) { return time < sample.time; });
    REQUIRE(after != samples.begin());
    return std::prev(after)->value;
}

int Transitions(bool (*predicate)(Outputs), int n0, int count) {
    int transitions = 0;
    for (int k = 1; k < count; ++k) {
        transitions += predicate(Expected(n0 + k)) != predicate(Expected(n0 + k - 1)) ? 1 : 0;
    }
    return transitions;
}

int RecordedTransitions(std::span<const Sample> samples, Time begin, Time end) {
    return static_cast<int>(std::ranges::count_if(
        samples, [&](const Sample& s) { return s.time > begin && s.time < end; }));
}

std::unique_ptr<LoadedCircuit> LoadTestPattern() {
    auto result = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

// Outputs are read where the monitor plugs in.
NetId VideoNet(const LoadedCircuit& loaded, std::string_view pin) {
    const auto net = ConnectorNet(loaded, "VIDEO", pin);
    REQUIRE(net.has_value());
    return *net;
}

// Start of the first vertical sync after `after`: a falling edge after a high interval longer
// than an HSYNC pulse, starting a low interval of at least 32 us.
Time FindVsyncStart(std::span<const Sample> samples, Time after) {
    for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
        const Sample& s = samples[i];
        if (s.time > after && s.value == Zero && samples[i - 1].value == One &&
            s.time - samples[i - 1].time > 6_us && samples[i + 1].time - s.time >= 32_us) {
            return s.time;
        }
    }
    FAIL("no vertical sync found");
    return 0;
}

}  // namespace

TEST_CASE("test pattern: loads without warnings", "[machines]") {
    const auto loaded = LoadTestPattern();
    CHECK(loaded->warnings.empty());
    CHECK(loaded->components.size() == 17);  // 15 chips, 2 connectors
}

TEST_CASE("test pattern: outputs match golden function", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    const auto loaded = LoadTestPattern();
    const NetId syncN = VideoNet(*loaded, "SYNC_N");
    const NetId video = VideoNet(*loaded, "VIDEO");
    Simulator sim(loaded->circuit);
    sim.powerOn(seed);
    sim.record(syncN);
    sim.record(video);
    sim.runFor(50_ms + 4 * kFrame);

    const auto syncSamples = sim.recorder().samples(syncN);
    const auto videoSamples = sim.recorder().samples(video);
    const Time t0 = FindVsyncStart(syncSamples, 50_ms);
    constexpr int n0 = 288 * kPixelsPerLine + 1;
    constexpr int count = 3 * kPixelsPerFrame;

    int mismatches = 0;
    std::string report;
    const auto check = [&](const char* name, LogicValue got, bool expected, int n) {
        if (got == (expected ? One : Zero)) {
            return;
        }
        if (++mismatches <= 10) {
            const int m = ((n - 1) % kPixelsPerFrame + kPixelsPerFrame) % kPixelsPerFrame + 1;
            report += std::format("frame {} line {} pixel {}: {} expected {} got {}\n",
                                  (n - n0) / kPixelsPerFrame, m / kPixelsPerLine,
                                  m % kPixelsPerLine, name, expected ? 1 : 0, ToChar(got));
        }
    };
    for (int k = 0; k < count; ++k) {
        const Time t = t0 + k * kPixel + kPixel / 2;
        const Outputs expected = Expected(n0 + k);
        check("SYNC_N", ValueAt(syncSamples, t), expected.syncN, n0 + k);
        check("VIDEO", ValueAt(videoSamples, t), expected.video, n0 + k);
    }
    INFO(report);
    REQUIRE(mismatches == 0);

    const Time t1 = t0 + count * kPixel;
    CHECK(RecordedTransitions(syncSamples, t0, t1) ==
          Transitions([](Outputs o) { return o.syncN; }, n0, count));
    CHECK(RecordedTransitions(videoSamples, t0, t1) ==
          Transitions([](Outputs o) { return o.video; }, n0, count));
}

TEST_CASE("test pattern: decode sanity", "[machines]") {
    for (int v = 0; v < kLinesPerFrame; ++v) {
        int syncPixels = 0;
        for (int h = 0; h < kPixelsPerLine; ++h) {
            syncPixels += Decode(h, v).syncN ? 0 : 1;
        }
        INFO("line " << v);
        CHECK(syncPixels == (v >= 288 && v <= 291 ? 60 : 4));
    }
    int white = 0;
    for (int n = 0; n < kPixelsPerFrame; ++n) {
        white += Decode(n % kPixelsPerLine, n / kPixelsPerLine).video ? 1 : 0;
    }
    CHECK(white == 2'484);
}

// TV_DESIGN §5.5: the frame starts 16 us into the vertical sync (line 288), so pixel (h, v),
// h < 52, lands on row v + 24, columns 8 (h + 12) + 0..7. Line 0 no longer follows the inverted
// sync pulses (P6n), so it is drawn where the others are.
TEST_CASE("test pattern: TV shows the grid", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    const auto loaded = LoadTestPattern();
    Simulator sim(loaded->circuit);
    sim.powerOn(seed);
    TvInput input(sim, VideoNet(*loaded, "VIDEO"), VideoNet(*loaded, "SYNC_N"));
    // Vertical pull-in slips 1 ms per free-running frame: up to ~16 frames from a bad phase.
    int ms = 0;
    for (; ms < 500 && !input.tv().isLocked(); ++ms) {
        sim.runFor(1_ms);
        input.update();
        sim.trimRecordings();
    }
    INFO("locked after " << ms << " ms");
    REQUIRE(input.tv().isLocked());
    const std::uint64_t frames = input.tv().completedFrames();
    while (input.tv().completedFrames() == frames) {
        sim.runFor(1_ms);
        input.update();
        sim.trimRecordings();
    }
    REQUIRE(input.tv().isLocked());
    const TvFrame& frame = input.tv().completedFrame();

    int mismatches = 0;
    std::string report;
    const auto check = [&](int row, int column, float expected, std::string_view what) {
        const float got = frame.at(row, column);
        if (got > expected - 1e-4f && got < expected + 1e-4f) {
            return;
        }
        if (++mismatches <= 10) {
            report += std::format("row {} column {} ({}): expected {} got {}\n", row, column, what,
                                  expected, got);
        }
    };
    for (int v = 0; v < 288; ++v) {
        for (int h = 0; h < 52; ++h) {
            const float expected = Decode(h, v).video ? 1.0f : 0.0f;
            for (int sub = 1; sub <= 6; ++sub) {
                check(v + 24, 8 * (h + 12) + sub, expected, std::format("v {} h {}", v, h));
            }
        }
    }
    for (int row = 0; row < kTvRows; ++row) {
        if (row >= 24 && row < 312) {
            continue;
        }
        for (int column = 0; column < kTvColumns; ++column) {
            check(row, column, 0.0f, "border");
        }
    }
    INFO(report);
    CHECK(mismatches == 0);

    // A monitor's window shows the grid but its lines 0-3 (rows 24-27), the overscan (P6n).
    const int lines = input.tv().completedStats().lines;
    const auto shown = LitBounds(frame, static_cast<int>(kWindowTop));
    REQUIRE(shown.has_value());
    CHECK(InsideWindow(*shown, lines));
    const auto all = LitBounds(frame);
    REQUIRE(all.has_value());
    CHECK(all->top == 24);
}
