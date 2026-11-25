#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <imgui.h>

#include "view/bench_places.hpp"
#include "view/time_view.hpp"

namespace bw {

// What the Oscilloscope and Logic Analyzer windows draw the same way.
inline constexpr ImU32 kTriggerMark = IM_COL32(255, 140, 0, 230);
inline constexpr ImU32 kMarkerLine = IM_COL32(255, 200, 0, 200);
inline constexpr ImU32 kCursorLine = IM_COL32(255, 255, 255, 90);
inline constexpr ImU32 kAxisText = IM_COL32(170, 175, 180, 255);
inline constexpr ImVec4 kHintGrey{0.5f, 0.5f, 0.5f, 1.0f};
inline constexpr float kAxisHeight = 18.0f;
// The Marker's flag at the top of its line: a click on it clears the Marker (P8f).
inline constexpr float kMarkerFlagWidth = 28.0f, kMarkerFlagHeight = 20.0f;
// The instrument windows' names, for the shelf to bring them to the front.
inline constexpr const char* kAnalyzerWindowName = "Logic Analyzer";
inline constexpr const char* kScopeWindowName = "Oscilloscope";

// ImGui::Begin for an instrument window, at its default place each time it comes out, normal size
// at first, with the minimal / normal size button in its title bar (P6d); End it as usual whatever
// this returns.
bool BeginInstrument(const char* name, bool& open, BenchObject object);
// After BeginInstrument: the left button went down in the window this frame.
bool InstrumentClicked();

using TimeToX = std::function<float(Time)>;  // ps from the Trigger → screen x

void DrawGrid(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float top,
              float bottom, ImU32 colour);
// Grid lines of `view` over `width` px, labelled as often as its widest label allows.
std::vector<GridLine> LabelledGrid(const TimeView& view, float width);
// Labels along an axis strip at `y`, each right of its line; a label not wholly inside
// [left, right], or under the off-screen Trigger's arrow, is left out.
void DrawAxisLabels(ImDrawList& list, const TimeView& view, const std::vector<GridLine>& grid,
                    const TimeToX& x, float y, float left, float right);
void DrawTrigger(ImDrawList& list, float x, float top, float bottom);  // line + triangle on top
// The Trigger off the view: an arrow at that end of the axis strip and its distance (nothing
// when the view holds it).
void DrawOffScreenTrigger(ImDrawList& list, const TimeView& view, float left, float right, float y);
void DrawMarker(ImDrawList& list, float x, float top, float bottom);  // line + "M" flag on top
// One line: the cursor, marker and dt, or `hint` in grey when there are neither.
void StatusLine(std::optional<Time> cursor, std::optional<Time> marker, const char* hint);

// Whole wheel notches from fractional wheel input (trackpads): `rest` keeps the remainder.
int WheelNotches(float wheel, float& rest);

}  // namespace bw
