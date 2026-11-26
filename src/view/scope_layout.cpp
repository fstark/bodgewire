#include "view/scope_layout.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace bw {

namespace {

constexpr float kMargin = 12;  // the strip's first group from the case's left edge
constexpr float kPad = 6;      // a group's panel round its keys
constexpr float kGap = 8;      // between groups, and between a key pair and the next
constexpr float kKeyGap = 2;   // between a key and its LED window
constexpr float kKey = 36, kKeyHigh = 44;
constexpr float kKeysTop = 22;  // under the groups' labels
constexpr float kTimeLed = 64, kPositionLed = 80;
constexpr float kRocker = 96, kLamp = 22;
constexpr float kLogoHeight = 32, kLogoWidth = kLogoHeight * 1000 / 622;
constexpr float kWords = 58;  // BODGE over WIRE, a gap before them included
constexpr float kSizeKey = 36;
constexpr float kBezelTop = 80, kBezelSide = 14, kBezelBottom = 12, kBezelInset = 8;

// ◀ value ▶ from `x`: the three boxes, and where they end.
float Stepper(float x, float led, Box& less, Box& value, Box& more) {
    const float y1 = kKeysTop + kKeyHigh;
    less = {x, kKeysTop, x + kKey, y1};
    value = {less.x1 + kKeyGap, kKeysTop, less.x1 + kKeyGap + led, y1};
    more = {value.x1 + kKeyGap, kKeysTop, value.x1 + kKeyGap + kKey, y1};
    return more.x1;
}

// The badge's logo at `x`, the words after it when `words`; then the time and trigger groups.
void LayOutStrip(ScopeCaseLayout& l, float x, bool words) {
    const float logoY = kKeysTop + (kKeyHigh - kLogoHeight) / 2;
    l.logo = {x, logoY, x + kLogoWidth, logoY + kLogoHeight};
    l.hasWords = words;
    l.words = words ? Box{l.logo.x1 + kGap, l.logo.y0, l.logo.x1 + kWords, l.logo.y1} : Box{};
    x = (words ? l.words.x1 : l.logo.x1) + kGap;
    const float groupTop = 6, groupBottom = kKeysTop + kKeyHigh + 4;
    const float timeX = x;
    x = Stepper(x + kPad, kTimeLed, l.timeLess, l.timeValue, l.timeMore);
    x = Stepper(x + kGap, kPositionLed, l.positionLess, l.positionValue, l.positionMore);
    l.timeGroup = {timeX, groupTop, x + kPad, groupBottom};
    x = l.timeGroup.x1 + kGap;
    const float rockerX = x + kPad, y1 = kKeysTop + kKeyHigh;
    l.autoHalf = {rockerX, kKeysTop, rockerX + kRocker / 2, y1};
    l.normHalf = {l.autoHalf.x1, kKeysTop, rockerX + kRocker, y1};
    const float lampY = kKeysTop + (kKeyHigh - kLamp) / 2;
    l.lamp = {l.normHalf.x1 + kGap, lampY, l.normHalf.x1 + kGap + kLamp, lampY + kLamp};
    l.triggerGroup = {x, groupTop, l.lamp.x1 + kPad, groupBottom};
}

}  // namespace

ScopeCaseLayout LayOutScopeCase(float width, float height) {
    ScopeCaseLayout l;
    const float sizeY = kKeysTop + (kKeyHigh - kSizeKey) / 2;
    l.sizeKey = {width - kScopeClose - 2 - kSizeKey, sizeY, width - kScopeClose - 2,
                 sizeY + kSizeKey};
    LayOutStrip(l, kMargin, true);
    if (l.triggerGroup.x1 + kGap > l.sizeKey.x0) {
        LayOutStrip(l, kMargin, false);
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

std::size_t BandAt(std::span<const Box> bands, float y) {
    assert(!bands.empty());
    std::size_t best = 0;
    for (std::size_t i = 1; i < bands.size(); ++i) {
        const auto distance = [&](const Box& b) { return std::abs((b.y0 + b.y1) / 2 - y); };
        if (distance(bands[i]) < distance(bands[best])) {
            best = i;
        }
    }
    return best;
}

}  // namespace bw
