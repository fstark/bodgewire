#include "devices/tv.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kLine = 64_us;
constexpr int kLinesPerFrame = 312;
constexpr Time kFrame = kLinesPerFrame * kLine;

// SYNC_N up to `end`: 4 us low at each line start (if horizontal); the first 3 lines of each
// frame low throughout (if vertical).
void FeedSync(Tv& tv, Time end, bool horizontal, bool vertical) {
    tv.setSyncN(0, One);
    for (int k = 0; k * kLine < end; ++k) {
        const Time t = k * kLine;
        if (vertical && k % kLinesPerFrame < 3) {
            tv.setSyncN(t, Zero);
        } else if (horizontal) {
            tv.setSyncN(t, Zero);
            tv.setSyncN(t + 4_us, One);
        } else {
            tv.setSyncN(t, One);
        }
    }
    tv.advanceTo(end);
}

}  // namespace

TEST_CASE("tv: 50 Hz sync locks", "[devices]") {
    Tv tv;
    FeedSync(tv, 200_ms, true, true);
    CHECK(tv.isLocked());
    const TvFrameStats& stats = tv.completedStats();
    CHECK(stats.verticalSynced);
    CHECK(stats.lines == kLinesPerFrame);
    CHECK(stats.syncedLines * 10 >= stats.lines * 9);
    const std::uint64_t frames = tv.completedFrames();
    Tv later;
    FeedSync(later, 200_ms + kFrame, true, true);
    CHECK(later.completedFrames() == frames + 1);
    CHECK(later.completedStats().start - stats.start == kFrame);
}

TEST_CASE("tv: missing vertical sync rolls", "[devices]") {
    Tv tv;
    FeedSync(tv, 200_ms, true, false);
    CHECK_FALSE(tv.isLocked());
    CHECK(tv.completedFrames() == 9);
    CHECK(tv.completedStats().start == 8 * 21_ms);
    CHECK_FALSE(tv.completedStats().verticalSynced);
}

TEST_CASE("tv: missing horizontal sync", "[devices]") {
    Tv tv;
    FeedSync(tv, 200_ms, false, true);
    CHECK_FALSE(tv.isLocked());
    const TvFrameStats& stats = tv.completedStats();
    CHECK(stats.verticalSynced);
    CHECK(stats.lines >= 300);
    CHECK(stats.lines <= 304);
    CHECK(stats.syncedLines <= 1);
}

TEST_CASE("tv: no signal", "[devices]") {
    Tv tv;
    tv.advanceTo(106_ms);
    CHECK(tv.completedFrames() == 5);
    CHECK_FALSE(tv.isLocked());
}

TEST_CASE("tv: sync at the free-running end is accepted", "[devices]") {
    Tv tv;
    tv.setSyncN(0, One);
    tv.setSyncN(2 * Tv::kLineFreeRun, Zero);
    tv.setSyncN(2 * Tv::kLineFreeRun + 4_us, One);
    tv.advanceTo(22_ms);
    CHECK(tv.completedFrames() == 1);
    CHECK(tv.completedStats().syncedLines == 1);
}

TEST_CASE("tv: early vertical sync is ignored", "[devices]") {
    Tv tv;
    tv.setSyncN(0, One);
    tv.setSyncN(10_ms, Zero);
    tv.setSyncN(10_ms + 100_us, One);
    tv.advanceTo(17_ms);
    CHECK(tv.completedFrames() == 0);
    tv.setSyncN(17_ms, Zero);
    tv.setSyncN(17_ms + 100_us, One);
    tv.advanceTo(18_ms);
    CHECK(tv.completedFrames() == 1);
    CHECK_FALSE(tv.completedStats().verticalSynced);
}
