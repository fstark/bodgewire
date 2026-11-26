#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "sim/time.hpp"

namespace bw {

// A time axis strip's height (the analyzer's and the scope's), px.
inline constexpr float kAxisHeight = 18.0f;

// What an instrument window shows: kDivisions divisions of a 1-2-5 step, from a left edge given
// in ps from the Trigger (negative before it). The analyzer's view over its Capture; the scope's
// settings (ScopeView, WithView).
struct TimeView {
    Time perDivision = 1'000'000;  // ps, a 1-2-5 step
    Time left = 0;                 // ps from the Trigger at the left edge
    Time span() const { return kDivisions * perDivision; }
    bool operator==(const TimeView&) const = default;
};

struct TimeRange {  // what a record holds, ps from the Trigger
    Time from = 0, to = 0;
    Time length() const { return to - from; }
};

Time StepAtLeast(Time t);  // smallest 1-2-5 step >= t (at least 1 ps)
Time StepUp(Time step);    // the next 1-2-5 step: 1 µs → 2 µs
Time StepDown(Time step);  // 1 µs → 500 ns; 1 ps stays

// `notches` > 0 zooms in (smaller steps), one step per notch; `anchor` (ps from T) stays at the
// same fraction of the width; the step stays within [minStep, maxStep].
TimeView ZoomAbout(TimeView view, Time anchor, int notches, Time minStep, Time maxStep);
TimeView Pan(TimeView view, double pixels, float width);  // > 0: the content moves right
TimeView Clamp(TimeView view, TimeRange record);          // inside when it fits, else centred
TimeView Fit(TimeRange record);                           // smallest step holding it, centred
TimeView FirstView(TimeRange record, Time period);        // 10 points per division about T, clamped
TimeView CentredOn(Time perDivision, Time centre);

struct GridLine {
    Time time;  // ps from T, a whole multiple of the step
    bool labelled;
    bool operator==(const GridLine&) const = default;
};
// Lines at whole multiples of the step inside the view; labels every 1, 2, 5 or 10 divisions,
// the fewest whose spacing reaches `labelGap` px.
std::vector<GridLine> Grid(const TimeView& view, float width, float labelGap);
// "-3 µs", "0", "+100.5 µs": with the digits the step needs.
std::string GridLabel(Time time, Time perDivision);

// Where a run's value text starts, or none: [x0, x1] the run's true ends (they may lie outside
// [left, right]); a slant only at an end inside the view; a cut run puts its text against the cut
// edge, so it never looks shorter than it is.
std::optional<float> RunTextX(float x0, float x1, float left, float right, float textWidth,
                              float slant);

// The scope's view is its settings: left = delay - timebase - lead × timebase (rounded). `lead`:
// divisions under the label plates, left of the division before the Trigger (P8h); 0: the
// Trigger at the first division.
TimeView ScopeView(const ScopeSettings& settings, double lead = 0);
// Back: delay = left + perDivision + lead × perDivision (rounded as ScopeView does, so a round
// trip is exact), then the timebase within kTimebases, the delay rounded to a tenth of a division
// and clamped to 0 … kMaxDelayDivisions divisions.
ScopeSettings WithView(ScopeSettings settings, const TimeView& view, double lead = 0);

// The scope's cursor tag (P8h): "+5 µs", "+5 µs   dt 3 µs" (the font has no Greek).
std::string CursorTagText(Time cursor, std::optional<Time> marker);

// `values` (ps, shortest first) by unit, for a drawn LED's grid (the scope's timebase, P8h; the
// analyzer's sample period, P8i): the ns, µs, ms and s rows that have values, each its indices
// into `values`.
struct UnitRow {
    const char* unit;
    std::vector<std::size_t> indices;
};
std::vector<UnitRow> UnitRows(std::span<const Time> values);

// The edge nearest `at` within `reach`, among `sorted` (ascending once through `proj`, in `at`'s
// unit); none out of reach. A finger's cursor and Marker land on it (P8f): the analyzer's run
// starts in points, the scope's Samples by their time.
template <std::ranges::random_access_range R, class Proj = std::identity>
std::optional<double> SnapToEdge(R&& sorted, double at, double reach, Proj proj = {}) {
    const auto value = [&](const auto& edge) {
        return static_cast<double>(std::invoke(proj, edge));
    };
    const auto after = std::ranges::lower_bound(sorted, at, {}, value);
    std::optional<double> best;
    const auto consider = [&](auto edge) {
        const double v = value(*edge);
        if (std::abs(v - at) <= reach && (!best || std::abs(v - at) < std::abs(*best - at))) {
            best = v;
        }
    };
    if (after != std::ranges::end(sorted)) {
        consider(after);
    }
    if (after != std::ranges::begin(sorted)) {
        consider(std::ranges::prev(after));
    }
    return best;
}

}  // namespace bw
