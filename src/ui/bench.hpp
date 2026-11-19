#pragma once

#include <imgui.h>

#include "view/bench_places.hpp"

namespace bw {

// The gold of what has the focus: the tool or instrument a click on a pin goes to (P6c); only
// that (P6o: the keys always go to the Machine).
inline constexpr ImU32 kFocusGold = IM_COL32(255, 196, 40, 255);

// The bench in screen coordinates: the main window's content above the shelf, where the Board is
// drawn and the equipment comes out (P6d).
struct BenchRect {
    ImVec2 min, max;
    Extent extent() const { return {max.x - min.x, max.y - min.y}; }
};
BenchRect BenchArea();

// Before Begin: on the bench (locked to the main window, clipped at its edge) or, popped out, in
// its own desktop window (P6j); at its default place and `first` its size the first time it comes
// out; after that where and as large as the player left it, for the run (FirstUseEver), unless
// TogglePop moved it.
void PlaceNext(BenchObject object, ImVec2 first);
// Before Begin: height = width * aspect, at least `minWidth` wide (a case keeps its proportions).
void KeepAspect(float aspect, float minWidth);
// The same, height = width * aspect * `stretch` (the binder, P6o): while the player drags the
// window's top or bottom edge (`window`: its Begin name), the height alone moves and sets
// `stretch` (kept within kMinStretch..kMaxStretch); the corners and sides scale in proportion.
inline constexpr float kMinStretch = 0.7f, kMaxStretch = 2.5f;
void KeepAspectStretched(float aspect, float minWidth, const char* window, float& stretch);
// A case's put-away mark, last before End: a small circled × in the window's top-right corner,
// faint until hovered. True when clicked.
bool CaseClose(float inset = 6);
// Inside a window, before drawing with a font other than the current one straight into a draw
// list (the keycap legends): baked at the window's pixel density, as PushFont does for the current
// font, else ImGui bakes it at 1x and a Retina display shows it blurred (P6l).
void SharpFont(ImFont* font);

// P6j: true while `object` is in its own desktop window, for the run.
bool PoppedOut(BenchObject object);
// The shelf's double-click (P6o): popped out, back on the bench where it was before it popped out
// (ReturnPlace), its size kept or shrunk in proportion to fit (FitBench); else to its own desktop
// window beside the main one (BesidePlace; else where it is), from its window's last place. From
// the next frame. `window`: its Begin name.
void TogglePop(BenchObject object, const char* window);
// --pop: popped out before the object first comes out, beside from its default place and first
// size (also TogglePop's, for a window never drawn).
void PopOutAtStart(BenchObject object);
// Before Begin of a window that never leaves the bench (the main one, the brief).
void OnBench();

}  // namespace bw
