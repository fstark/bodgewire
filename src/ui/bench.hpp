#pragma once

#include <optional>

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
// out, shrunk in proportion on a bench smaller than it (P8f); after that where and as large as
// the player left it, for the bench (FirstUseEver), unless TogglePop or a fill moved it.
void PlaceNext(BenchObject object, ImVec2 first);
// Before Begin: height = width * aspect, at least `minWidth` wide (a case keeps its proportions).
void KeepAspect(float aspect, float minWidth);
// A case's put-away mark, last before End: a circled × in the window's top-right corner, always
// seen, brighter when hovered; its item the corner's 40 px (P8f). True when clicked.
bool CaseClose(float inset = 6);
// Inside a window, before drawing with a font other than the current one straight into a draw
// list (the keycap legends): baked at the window's pixel density, as PushFont does for the current
// font, else ImGui bakes it at 1x and a Retina display shows it blurred (P6l).
void SharpFont(ImFont* font);

// P6j: true while `object` is in its own desktop window, for the bench.
bool PoppedOut(BenchObject object);
// The shelf's double-click (P6o): popped out, back on the bench where it was before it popped out
// (ReturnPlace), its size kept or shrunk in proportion to fit (FitBench); else to its own desktop
// window beside the main one (BesidePlace; else where it is), from its window's last place. From
// the next frame. `window`: its Begin name. Where there are no desktop windows (the web build,
// --one-window; P8f) it fills the bench instead (the largest of its shape for an object that
// keeps its proportions, centred); again, it goes back where and as large as it was.
void TogglePop(BenchObject object, const char* window);
// Inside `object`'s window: it fills the bench from the next frame (an Instrument's full size).
void FillBenchWith(BenchObject object);
// Back where it was before it filled the bench, `size` its size there (nullopt: the one it
// had); nothing for an object that does not fill it.
void Unfill(BenchObject object, std::optional<Extent> size = std::nullopt);
bool Filled(BenchObject object);
// Inside a window, last before End: a double-click on its drag zone (its title bar, or its own
// body off every control: where a drag moves it; not inside a child, a binder's page) pops it out,
// or back in (P6o), as a double-click on its shelf drawing does.
void PopOnDoubleClick(BenchObject object);
// --pop: popped out before the object first comes out, beside from its default place and first
// size (also TogglePop's, for a window never drawn).
void PopOutAtStart(BenchObject object);
// Before Begin of a window that never leaves the bench (the main one, the brief).
void OnBench();
// A fresh bench (P7a): nothing popped out, no place remembered; first in each bench.
void ResetBench();

}  // namespace bw
