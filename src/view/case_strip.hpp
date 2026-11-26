#pragma once

#include <cstddef>
#include <span>

namespace bw {

struct Box {  // px
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float width() const { return x1 - x0; }
    float height() const { return y1 - y0; }
    bool contains(float x, float y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
    bool overlaps(const Box& other) const {
        return x0 < other.x1 && other.x0 < x1 && y0 < other.y1 && other.y0 < y1;
    }
};

// The box whose centre is nearest `y` (a drop's target); `boxes` not empty.
std::size_t BandAt(std::span<const Box> boxes, float y);

// A drawn case's strip of keys (P8h, P8i), px from the window's top left: tinted groups of keys
// kBig (44 px) high and 36 wide, for a finger, labelled above them.
inline constexpr float kCaseStrip = 72;    // the strip's height
inline constexpr float kCaseClose = 40;    // CaseClose's corner, px square
inline constexpr float kStripMargin = 12;  // the first group from the case's left edge
inline constexpr float kStripPad = 6;      // a group's panel round its keys
inline constexpr float kStripGap = 8;      // between groups, and between a key pair and the next
inline constexpr float kStripKeyGap = 2;   // between a key and its LED window
inline constexpr float kStripKey = 36, kStripKeyHigh = 44;
inline constexpr float kStripKeysTop = 22;  // under the groups' labels
inline constexpr float kGroupTop = 6, kGroupBottom = kStripKeysTop + kStripKeyHigh + 4;
inline constexpr float kSizeKey = 36;

// ◀ value ▶ from `x`, `led` px wide between the keys: the three boxes; returns where they end.
float LayOutStepper(float x, float led, Box& less, Box& value, Box& more);

// The maker's badge: the BW logo, 32 px high, and BODGE over WIRE beside it when `hasWords`.
struct Badge {
    Box logo, words;
    bool hasWords = false;
};
// From `x`, the words when `words`; returns where it ends.
float LayOutBadge(float x, bool words, Badge& badge);
// The size key (minimal / normal / full), left of CaseClose's corner, in a case `width` wide.
Box SizeKeyBox(float width);

}  // namespace bw
