#include "view/case_strip.hpp"

#include <cassert>
#include <cmath>

namespace bw {

namespace {

constexpr float kLogoHeight = 32, kLogoWidth = kLogoHeight * 1000 / 622;
constexpr float kWords = 58;  // BODGE over WIRE, a gap before them included

}  // namespace

std::size_t BandAt(std::span<const Box> boxes, float y) {
    assert(!boxes.empty());
    std::size_t best = 0;
    for (std::size_t i = 1; i < boxes.size(); ++i) {
        const auto distance = [&](const Box& b) { return std::abs((b.y0 + b.y1) / 2 - y); };
        if (distance(boxes[i]) < distance(boxes[best])) {
            best = i;
        }
    }
    return best;
}

float LayOutStepper(float x, float led, Box& less, Box& value, Box& more) {
    const float y1 = kStripKeysTop + kStripKeyHigh;
    less = {x, kStripKeysTop, x + kStripKey, y1};
    value = {less.x1 + kStripKeyGap, kStripKeysTop, less.x1 + kStripKeyGap + led, y1};
    more = {value.x1 + kStripKeyGap, kStripKeysTop, value.x1 + kStripKeyGap + kStripKey, y1};
    return more.x1;
}

float LayOutBadge(float x, bool words, Badge& badge) {
    const float logoY = kStripKeysTop + (kStripKeyHigh - kLogoHeight) / 2;
    badge.logo = {x, logoY, x + kLogoWidth, logoY + kLogoHeight};
    badge.hasWords = words;
    badge.words =
        words ? Box{badge.logo.x1 + kStripGap, badge.logo.y0, badge.logo.x1 + kWords, badge.logo.y1}
              : Box{};
    return words ? badge.words.x1 : badge.logo.x1;
}

Box SizeKeyBox(float width) {
    const float y = kStripKeysTop + (kStripKeyHigh - kSizeKey) / 2;
    return {width - kCaseClose - 2 - kSizeKey, y, width - kCaseClose - 2, y + kSizeKey};
}

}  // namespace bw
