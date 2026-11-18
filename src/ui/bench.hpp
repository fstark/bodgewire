#pragma once

#include <imgui.h>

#include "view/bench_places.hpp"

namespace bw {

// The gold of what has the focus: the tool or instrument a click on a pin goes to (P6c), and
// the Machine's objects while the keys go to it (P6g).
inline constexpr ImU32 kFocusGold = IM_COL32(255, 196, 40, 255);

// The bench in screen coordinates: the main window's content above the shelf, where the Board is
// drawn and the equipment comes out (P6d).
struct BenchRect {
    ImVec2 min, max;
    Extent extent() const { return {max.x - min.x, max.y - min.y}; }
};
BenchRect BenchArea();

// Before Begin: on the bench (locked to the main window, clipped at its edge) or, popped out, in
// its own desktop window (P6j); then and `first` its size the first time it comes
// out; after that where and as large as the player left it, for the run (FirstUseEver), unless
// SendHome asked for its default place again.
void PlaceNext(BenchObject object, ImVec2 first);
// The next PlaceNext of `object` puts it back at its default place (its shelf drawing
// double-clicked), keeping its size.
void SendHome(BenchObject object);
// Before Begin: height = width * aspect, at least `minWidth` wide (a case keeps its proportions).
void KeepAspect(float aspect, float minWidth);
// A case's put-away mark, last before End: a small circled × in the window's top-right corner,
// faint until hovered. True when clicked.
bool CaseClose(float inset = 6);

// P6j: true while `object` is in its own desktop window, for the run.
bool PoppedOut(BenchObject object);
// --pop: `object` in its own desktop window from its first PlaceNext.
void PopOutAtStart(BenchObject object);
// Before Begin of a window that never leaves the bench (the main one, the brief).
void OnBench();

}  // namespace bw
