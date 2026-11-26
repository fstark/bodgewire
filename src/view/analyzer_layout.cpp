#include "view/analyzer_layout.hpp"

#include <algorithm>

#include "view/time_view.hpp"

namespace bw {

namespace {

constexpr float kArm = 48;
constexpr float kLed = 64;
constexpr float kFit = 44;
constexpr float kBezelTop = 80, kBezelSide = 14, kBezelBottom = 12, kBezelInset = 8;
// The screen's top rows: the corner's keys, the overview, the axis.
constexpr float kCornerKeyTop = 3, kCornerKeyHigh = 28, kPinBus = 40, kGroupKey = 72;
constexpr float kOverviewTop = 4, kOverviewHigh = 10, kAxisTop = 16;
constexpr float kRowsTop = kAxisTop + kAxisHeight;  // 34

// The groups after the badge, from `x`.
void LayOutGroups(AnalyzerCaseLayout& l, float x) {
    const float y0 = kStripKeysTop, y1 = kStripKeysTop + kStripKeyHigh;
    // CAPTURE: ARM, lit by the phase.
    l.armKey = {x + kStripPad, y0, x + kStripPad + kArm, y1};
    l.captureGroup = {x, kGroupTop, l.armKey.x1 + kStripPad, kGroupBottom};
    // SAMPLE: ◀ LED ▶.
    x = l.captureGroup.x1 + kStripGap;
    const float sampleEnd =
        LayOutStepper(x + kStripPad, kLed, l.sampleLess, l.sampleValue, l.sampleMore);
    l.sampleGroup = {x, kGroupTop, sampleEnd + kStripPad, kGroupBottom};
    // VIEW / DIV: ◀ LED ▶, FIT.
    x = l.sampleGroup.x1 + kStripGap;
    const float viewEnd = LayOutStepper(x + kStripPad, kLed, l.viewLess, l.viewValue, l.viewMore);
    l.fitKey = {viewEnd + kStripGap, y0, viewEnd + kStripGap + kFit, y1};
    l.viewGroup = {x, kGroupTop, l.fitKey.x1 + kStripPad, kGroupBottom};
}

}  // namespace

AnalyzerCaseLayout LayOutAnalyzerCase(float width, float height) {
    AnalyzerCaseLayout l;
    l.sizeKey = SizeKeyBox(width);
    // The badge first, with its words when the strip still fits left of the size key.
    for (const bool words : {true, false}) {
        LayOutGroups(l, LayOutBadge(kStripMargin, words, l.badge) + kStripGap);
        if (l.viewGroup.x1 + kStripGap <= l.sizeKey.x0) {
            break;
        }
    }
    l.bezel = {kBezelSide, kBezelTop, width - kBezelSide, height - kBezelBottom};
    l.screen = {l.bezel.x0 + kBezelInset, l.bezel.y0 + kBezelInset, l.bezel.x1 - kBezelInset,
                l.bezel.y1 - kBezelInset};
    const Box& s = l.screen;
    l.laneX0 = s.x0 + kAnalyzerPlate;
    const float keyY0 = s.y0 + kCornerKeyTop, keyY1 = keyY0 + kCornerKeyHigh;
    l.pinHalf = {s.x0 + 6, keyY0, s.x0 + 6 + kPinBus, keyY1};
    l.busHalf = {l.pinHalf.x1, keyY0, l.pinHalf.x1 + kPinBus, keyY1};
    l.groupKey = {l.busHalf.x1 + 8, keyY0, l.busHalf.x1 + 8 + kGroupKey, keyY1};
    l.count = {l.groupKey.x1 + 8, keyY0, l.laneX0 - 8, keyY1};
    l.overview = {l.laneX0 + 4, s.y0 + kOverviewTop, s.x1 - 4, s.y0 + kOverviewTop + kOverviewHigh};
    l.axis = {l.laneX0, s.y0 + kAxisTop, s.x1, s.y0 + kRowsTop};
    l.rows = {s.x0, s.y0 + kRowsTop, s.x1, s.y1};
    return l;
}

float AnalyzerRowHeight(float room, std::size_t count) {
    if (count == 0) {
        return kRowMax;
    }
    return std::clamp(room / static_cast<float>(count) - kRowGap, kRowMin, kRowMax);
}

}  // namespace bw
