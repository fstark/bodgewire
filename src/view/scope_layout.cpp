#include "view/scope_layout.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace bw {

namespace {

constexpr float kTimeLed = 64, kPositionLed = 80;
constexpr float kRocker = 96, kLamp = 22;
constexpr float kBezelTop = 80, kBezelSide = 14, kBezelBottom = 12, kBezelInset = 8;

// The time and trigger groups after the badge, from `x`.
void LayOutStrip(ScopeCaseLayout& l, float x) {
    const float timeX = x;
    x = LayOutStepper(x + kStripPad, kTimeLed, l.timeLess, l.timeValue, l.timeMore);
    x = LayOutStepper(x + kStripGap, kPositionLed, l.positionLess, l.positionValue, l.positionMore);
    l.timeGroup = {timeX, kGroupTop, x + kStripPad, kGroupBottom};
    x = l.timeGroup.x1 + kStripGap;
    const float rockerX = x + kStripPad, y1 = kStripKeysTop + kStripKeyHigh;
    l.autoHalf = {rockerX, kStripKeysTop, rockerX + kRocker / 2, y1};
    l.normHalf = {l.autoHalf.x1, kStripKeysTop, rockerX + kRocker, y1};
    const float lampY = kStripKeysTop + (kStripKeyHigh - kLamp) / 2;
    l.lamp = {l.normHalf.x1 + kStripGap, lampY, l.normHalf.x1 + kStripGap + kLamp, lampY + kLamp};
    l.triggerGroup = {x, kGroupTop, l.lamp.x1 + kStripPad, kGroupBottom};
}

}  // namespace

ScopeCaseLayout LayOutScopeCase(float width, float height) {
    ScopeCaseLayout l;
    l.sizeKey = SizeKeyBox(width);
    // The badge first, with its words when the strip still fits left of the size key.
    for (const bool words : {true, false}) {
        Badge badge;
        LayOutStrip(l, LayOutBadge(kStripMargin, words, badge) + kStripGap);
        l.logo = badge.logo;
        l.words = badge.words;
        l.hasWords = badge.hasWords;
        if (l.triggerGroup.x1 + kStripGap <= l.sizeKey.x0) {
            break;
        }
    }
    l.bezel = {kBezelSide, kBezelTop, width - kBezelSide, height - kBezelBottom};
    l.screen = {l.bezel.x0 + kBezelInset, l.bezel.y0 + kBezelInset, l.bezel.x1 - kBezelInset,
                l.bezel.y1 - kBezelInset};
    return l;
}

std::vector<Box> ScopeBands(const Box& screen, float axis, std::size_t count) {
    std::vector<Box> bands;
    if (count == 0) {
        return bands;
    }
    const float top = screen.y0 + axis;
    const auto n = static_cast<float>(count);
    const float height = std::max(kMinBand, (screen.y1 - top - kBandGap * (n - 1)) / n);
    for (std::size_t i = 0; i < count; ++i) {
        const float y0 = top + static_cast<float>(i) * (height + kBandGap);
        bands.push_back({screen.x0, y0, screen.x1, y0 + height});
    }
    return bands;
}

}  // namespace bw
