#include "view/time_view.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

namespace {

// 65 536 points at 100 ns with the Trigger at 10 % (point 6553).
constexpr TimeRange kRecord{.from = -6553 * 100_ns, .to = (65'536 - 6553) * 100_ns};

}  // namespace

TEST_CASE("time view: 1-2-5 steps", "[view]") {
    CHECK(StepUp(1_us) == 2_us);
    CHECK(StepUp(2_us) == 5_us);
    CHECK(StepUp(5_us) == 10_us);
    CHECK(StepDown(1_us) == 500_ns);
    CHECK(StepDown(2_us) == 1_us);
    CHECK(StepDown(1) == 1);
    CHECK(StepAtLeast(1'600'000) == 2_us);
    CHECK(StepAtLeast(650_us) == 1_ms);
    CHECK(StepAtLeast(1_us) == 1_us);
    CHECK(StepAtLeast(0) == 1);
}

TEST_CASE("time view: zoom keeps the anchor in place", "[view]") {
    const TimeView view{.perDivision = 1_us, .left = -3'500'000};
    const Time anchor = 3'700'000;  // 72 % of the width
    const TimeView in = ZoomAbout(view, anchor, 1, 1_ns, 1_ms);
    CHECK(in == TimeView{.perDivision = 500_ns, .left = 100_ns});
    const TimeView out = ZoomAbout(view, anchor, -2, 1_ns, 1_ms);
    CHECK(out == TimeView{.perDivision = 5_us, .left = anchor - 36_us});
    CHECK(ZoomAbout(view, anchor, 3, 1_us, 1_ms) == view);  // at the limit
    CHECK(ZoomAbout(view, anchor, -30, 1_ns, 2_us).perDivision == 2_us);
}

TEST_CASE("time view: limits", "[view]") {
    const TimeView view{.perDivision = 1_us, .left = 0};
    CHECK(Clamp(Pan(view, -1e9, 1000), kRecord).left == kRecord.to - 10_us);
    CHECK(Clamp(Pan(view, 1e9, 1000), kRecord).left == kRecord.from);
    CHECK(Pan(view, 100, 1000).left == -1_us);  // a tenth of the width: one division
    const TimeView wide = Clamp({.perDivision = 1_ms, .left = 0}, kRecord);
    CHECK(wide.left == kRecord.from - (10_ms - kRecord.length()) / 2);
}

TEST_CASE("time view: fit and first view", "[view]") {
    const TimeView fit = Fit(kRecord);
    CHECK(fit.perDivision == 1_ms);
    CHECK(fit.left == -2'378'500'000);
    CHECK(FirstView(kRecord, 100_ns) == TimeView{.perDivision = 1_us, .left = -5_us});
    CHECK(FirstView({-6553 * 10_ns, 58'983 * 10_ns}, 10_ns) ==
          TimeView{.perDivision = 100_ns, .left = -500_ns});
    CHECK(FirstView({-6553 * 1_ms, 58'983 * 1_ms}, 1_ms) ==
          TimeView{.perDivision = 10_ms, .left = -50_ms});
    // The Trigger at point 3: clamped to the record's start.
    CHECK(FirstView({-300_ns, 65'533 * 100_ns}, 100_ns).left == -300_ns);
}

TEST_CASE("time view: grid anchored to the Trigger", "[view]") {
    const TimeView view{.perDivision = 1_us, .left = -3'500'000};
    const auto wide = Grid(view, 660, 60);
    REQUIRE(wide.size() == 10);
    CHECK(wide.front() == GridLine{.time = -3_us, .labelled = true});
    CHECK(wide.back() == GridLine{.time = 6_us, .labelled = true});
    for (const GridLine& line : wide) {
        CHECK(line.labelled);
    }
    const auto narrow = Grid(view, 200, 60);  // 20 px a division: a label every 5
    for (const GridLine& line : narrow) {
        CHECK(line.labelled == (line.time == 0 || line.time == 5_us));
    }
    const auto later = Grid({.perDivision = 1_us, .left = 3'500'000}, 660, 60);
    CHECK(later.front().time == 4_us);
    CHECK(GridLabel(-3_us, 1_us) == "-3 µs");
    CHECK(GridLabel(0, 1_us) == "0");
    CHECK(GridLabel(6_us, 1_us) == "+6 µs");
    // Neighbours never print alike: 100 and 100.5 µs at 500 ns a division.
    CHECK(GridLabel(100_us, 500_ns) == "+100 µs");
    CHECK(GridLabel(100'500'000, 500_ns) == "+100.5 µs");
    CHECK(GridLabel(2'500'000'000, 500_us) == "+2.5 ms");
}

TEST_CASE("time view: off-screen Trigger", "[view]") {
    const ScopeSettings delayed{.timebase = 200_ns, .delay = 1_us};
    CHECK(OffScreenTrigger(ScopeView(delayed)) == -800_ns);
    CHECK(OffScreenTrigger(ScopeView(ScopeSettings{})) == std::nullopt);
    CHECK(OffScreenTrigger({.perDivision = 1_us, .left = -12_us}) == 2_us);
}

TEST_CASE("time view: value text of a cut run", "[view]") {
    // View from 100 to 500 px, text 40 px wide, slants 3 px.
    CHECK(RunTextX(50, 200, 100, 500, 40, 3) == 104.0f);   // cut left: against the left edge
    CHECK(RunTextX(400, 600, 100, 500, 40, 3) == 456.0f);  // cut right: against the right edge
    CHECK(RunTextX(0, 600, 100, 500, 40, 3) == 280.0f);    // cut on both: centred in the view
    CHECK(RunTextX(200, 300, 100, 500, 40, 3) == 230.0f);  // whole: centred
    CHECK(RunTextX(200, 240, 100, 500, 40, 3) == std::nullopt);
    CHECK(RunTextX(20, 120, 100, 500, 40, 3) == std::nullopt);  // 20 px visible
    CHECK(RunTextX(0, 90, 100, 500, 40, 3) == std::nullopt);    // off screen
}

TEST_CASE("time view: the scope's settings from a view", "[view]") {
    // Zooming out about +1 µs at delay 0 would need a negative delay: it stays 0.
    const ScopeSettings start{.timebase = 200_ns};
    const ScopeSettings out = WithView(
        start, ZoomAbout(ScopeView(start), 1_us, -1, kTimebases.front(), kTimebases.back()));
    CHECK(out.timebase == 500_ns);
    CHECK(out.delay == 0);
    // Zooming in about +1.5 µs with a 1 µs delay keeps +1.5 µs at 35 % of the width.
    const ScopeSettings delayed{.timebase = 200_ns, .delay = 1_us};
    const ScopeSettings in = WithView(delayed, ZoomAbout(ScopeView(delayed), 1'500'000, 1,
                                                         kTimebases.front(), kTimebases.back()));
    CHECK(in.timebase == 100_ns);
    CHECK(in.delay == 1'250'000);
    // A drag snaps the delay to a tenth of a division (20 ns).
    CHECK(WithView(delayed, Pan(ScopeView(delayed), -33, 1000)).delay == 1'060'000);
    CHECK(WithView(delayed, {.perDivision = 200_ns, .left = 1_s}).delay ==
          kMaxDelayDivisions * 200_ns);
}

TEST_CASE("time view: a slow scope drag moves from the press", "[view]") {
    // 2 px a frame over 1000 px is 4 ns: rounded to the 20 ns tenth, frame by frame it never moves.
    const ScopeSettings press{.timebase = 200_ns, .delay = 1_us};
    ScopeSettings perFrame = press, fromPress = press;
    for (int frame = 1; frame <= 10; ++frame) {
        perFrame = WithView(perFrame, Pan(ScopeView(perFrame), -2, 1000));
        fromPress = WithView(press, Pan(ScopeView(press), -2.0 * frame, 1000));
    }
    CHECK(perFrame.delay == 1_us);
    CHECK(fromPress.delay == 1'040'000);
}

TEST_CASE("time view: status line", "[view]") {
    CHECK(StatusText(std::nullopt, std::nullopt).empty());
    CHECK(StatusText(3'700'000, std::nullopt) == "cursor +3.7 µs");
    CHECK(StatusText(std::nullopt, 2_us) == "M +2 µs");
    CHECK(StatusText(3'700'000, 2_us) == "cursor +3.7 µs   M +2 µs   dt 1.7 µs (588 kHz)");
    CHECK(StatusText(2_us, 2_us) == "cursor +2 µs   M +2 µs   dt 0");
}
