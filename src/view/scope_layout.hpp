#pragma once

#include <cstddef>
#include <span>
#include <vector>

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

// The Oscilloscope's case (P8h), px from the window's top left: the strip of keys along its top,
// the screen in its bezel under it. Keys kBig (44 px) high, 36 wide, for a finger.
struct ScopeCaseLayout {
    Box timeGroup, triggerGroup;                    // the strip's tinted panels
    Box timeLess, timeValue, timeMore;              // TIME / DIV ◀ value ▶
    Box positionLess, positionValue, positionMore;  // POSITION ◀ value ▶
    Box autoHalf, normHalf, lamp;                   // the rocker's halves, TRIG'D
    Box logo;                                       // the BW logo
    Box words;                                      // BODGE over WIRE, when `hasWords`
    bool hasWords = false;
    Box sizeKey;  // minimal / normal / full; left of CaseClose's corner
    Box bezel, screen;
};
inline constexpr float kScopeStrip = 72;  // the strip's height
inline constexpr float kScopeClose = 40;  // CaseClose's corner, px square
inline constexpr float kScopeMinWidth = 620, kScopeMinHeight = 400;
ScopeCaseLayout LayOutScopeCase(float width, float height);

inline constexpr float kPlate = 156;   // a band's label plate, px
inline constexpr float kBandGap = 6;   // black between two bands
inline constexpr float kMinBand = 50;  // a band's least height: a 28 px row and a text line

// The bands' boxes in `screen` below its axis strip (`axis` px high), top to bottom, the height
// shared, each at least kMinBand.
std::vector<Box> ScopeBands(const Box& screen, float axis, std::size_t count);
// The band whose centre is nearest `y` (a drop's target); `bands` not empty.
std::size_t BandAt(std::span<const Box> bands, float y);

}  // namespace bw
