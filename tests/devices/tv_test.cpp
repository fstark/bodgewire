#include "devices/tv.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kLine = 64_us;
constexpr int kLinesPerFrame = 312;
constexpr Time kFrame = kLinesPerFrame * kLine;

// SYNC_N for the lines starting in [from, end): 4 us low at each line start (if horizontal); the
// first 3 lines of each frame low throughout (if vertical).
void FeedSync(Tv& tv, Time from, Time end, bool horizontal, bool vertical) {
    if (from == 0) {
        tv.setSyncN(0, One);
    }
    for (Time k = (from + kLine - 1) / kLine; k * kLine < end; ++k) {
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

void FeedSync(Tv& tv, Time end, bool horizontal, bool vertical) {
    FeedSync(tv, 0, end, horizontal, vertical);
}

bool Near(float a, float b) { return a > b - 1e-4f && a < b + 1e-4f; }

// Locked 50 Hz sync with `video` called at the start of line 1600 (row 40 of its frame); returns
// once that frame has completed.
template <typename Video>
Tv PaintLine(Video video) {
    constexpr Time kStart = 1600 * kLine;
    Tv tv;
    FeedSync(tv, kStart, true, true);
    FeedSync(tv, kStart, kStart + 5_us, true, true);
    video(tv, kStart);
    FeedSync(tv, kStart + 5_us, 1872 * kLine + 20_us, true, true);
    return tv;
}

}  // namespace

TEST_CASE("tv: video paints its columns", "[devices]") {
    const Tv tv = PaintLine([](Tv& t, Time line) {
        t.setVideo(line + 10_us, One);
        t.setVideo(line + 20_us, Zero);
    });
    REQUIRE(tv.isLocked());
    const TvFrame& frame = tv.completedFrame();
    for (int column = 80; column < 160; ++column) {
        INFO("column " << column);
        CHECK(Near(frame.at(40, column), 1.0f));
    }
    CHECK(frame.at(40, 79) == 0.0f);
    CHECK(frame.at(40, 160) == 0.0f);
    CHECK(frame.at(39, 100) == 0.0f);
    CHECK(frame.at(41, 100) == 0.0f);
}

TEST_CASE("tv: X paints grey, half a column paints half", "[devices]") {
    const Tv tv = PaintLine([](Tv& t, Time line) {
        t.setVideo(line + 10_us, X);
        t.setVideo(line + 20_us, Zero);
        t.setVideo(line + 30_us, One);
        t.setVideo(line + 30_us + 62'500, Zero);
    });
    const TvFrame& frame = tv.completedFrame();
    CHECK(Near(frame.at(40, 100), 0.5f));
    CHECK(Near(frame.at(40, 240), 0.5f));
    CHECK(frame.at(40, 241) == 0.0f);
}

TEST_CASE("tv: no signal gives black frames", "[devices]") {
    Tv tv;
    tv.advanceTo(50_ms);
    REQUIRE(tv.completedFrames() == 2);
    for (const float cell : tv.completedFrame().cells) {
        REQUIRE(cell == 0.0f);
    }
}

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
