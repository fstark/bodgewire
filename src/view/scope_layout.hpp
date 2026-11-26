#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "view/case_strip.hpp"

namespace bw {

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
inline constexpr float kScopeMinWidth = 620, kScopeMinHeight = 400;
ScopeCaseLayout LayOutScopeCase(float width, float height);

inline constexpr float kPlate = 156;   // a band's label plate, px
inline constexpr float kBandGap = 6;   // black between two bands
inline constexpr float kMinBand = 50;  // a band's least height: a 28 px row and a text line

// The bands' boxes in `screen` below its axis strip (`axis` px high), top to bottom, the height
// shared, each at least kMinBand.
std::vector<Box> ScopeBands(const Box& screen, float axis, std::size_t count);

}  // namespace bw
