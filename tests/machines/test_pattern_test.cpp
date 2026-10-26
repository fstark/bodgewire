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

#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

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
    const bool vsync = v >= 308 && v <= 311;
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
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
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
    CHECK(loaded->components.size() == 15);
}

TEST_CASE("test pattern: outputs match golden function", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    const auto loaded = LoadTestPattern();
    const NetId syncN = loaded->nets.at("SYNC_N");
    const NetId video = loaded->nets.at("VIDEO");
    Simulator sim(loaded->circuit);
    sim.powerOn(seed);
    sim.record(syncN);
    sim.record(video);
    sim.runFor(50_ms + 4 * kFrame);

    const auto syncSamples = sim.recorder().samples(syncN);
    const auto videoSamples = sim.recorder().samples(video);
    const Time t0 = FindVsyncStart(syncSamples, 50_ms);
    constexpr int n0 = 308 * kPixelsPerLine + 1;
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
        CHECK(syncPixels == (v >= 308 ? 60 : 4));
    }
    int white = 0;
    for (int n = 0; n < kPixelsPerFrame; ++n) {
        white += Decode(n % kPixelsPerLine, n / kPixelsPerLine).video ? 1 : 0;
    }
    CHECK(white == 2'484);
}
