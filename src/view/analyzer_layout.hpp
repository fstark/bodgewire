#pragma once

#include <array>
#include <cstddef>

#include "view/case_strip.hpp"

namespace bw {

// The Logic Analyzer's case (P8i), px from the window's top left: the strip (the badge, CAPTURE,
// SAMPLE, TRIGGER AT, VIEW / DIV, the size key), the screen in its bezel under it. The screen's
// corner above the plates holds PIN | BUS, GROUP and the count of Clips; the overview and the time
// axis are above the lanes; the rows under them, to the screen's bottom.
struct AnalyzerCaseLayout {
    Badge badge;
    Box captureGroup, armKey;
    std::array<Box, 3> lamps;  // ARMED, TRIG'D, DONE
    Box sampleGroup, sampleLess, sampleValue, sampleMore;
    Box triggerAtGroup;
    std::array<Box, 3> triggerAt;  // EARLY, MID, LATE
    Box viewGroup, viewLess, viewValue, viewMore, fitKey;
    Box sizeKey;
    Box bezel, screen;
    Box pinHalf, busHalf, groupKey, count;  // the corner above the plates
    Box overview, axis;                     // above the lanes
    Box rows;                               // plates and lanes, to the screen's bottom
    float laneX0 = 0;                       // the lanes' left: screen.x0 + kAnalyzerPlate
};
inline constexpr float kAnalyzerMinWidth = 880, kAnalyzerMinHeight = 340;
inline constexpr float kAnalyzerPlate = 248;  // a row's plate: tick, name, value, Trigger, ×
inline constexpr float kRowMin = 28, kRowMax = 56, kRowGap = 2;
AnalyzerCaseLayout LayOutAnalyzerCase(float width, float height);

// A row's height for `count` rows in `room` px, each with kRowGap of black under it: the room
// shared, from kRowMin (then the rows scroll) to kRowMax.
float AnalyzerRowHeight(float room, std::size_t count);

}  // namespace bw
