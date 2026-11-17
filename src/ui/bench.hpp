#pragma once

#include <imgui.h>

#include "view/bench_places.hpp"

namespace bw {

// The bench in screen coordinates: the main window's content above the shelf, where the Board is
// drawn and the equipment comes out (P6d).
struct BenchRect {
    ImVec2 min, max;
    Extent extent() const { return {max.x - min.x, max.y - min.y}; }
};
BenchRect BenchArea();

// Before Begin: the window at its default place each time it comes out (ImGuiCond_Appearing),
// `first` its size the first time only (FirstUseEver: the player's size is kept for the run).
void PlaceNext(BenchObject object, ImVec2 first);
// Before Begin: height = width * aspect, at least `minWidth` wide (a case keeps its proportions).
void KeepAspect(float aspect, float minWidth);
// A case's put-away mark, last before End: a small circled × in the window's top-right corner,
// faint until hovered. True when clicked.
bool CaseClose(float inset = 6);

}  // namespace bw
