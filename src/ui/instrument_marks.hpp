#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <imgui.h>

#include "view/time_view.hpp"

namespace bw {

// What the Oscilloscope and Logic Analyzer windows draw the same way.
inline constexpr ImU32 kTriggerMark = IM_COL32(255, 140, 0, 230);
inline constexpr ImU32 kMarkerLine = IM_COL32(255, 200, 0, 200);
inline constexpr ImU32 kCursorLine = IM_COL32(255, 255, 255, 90);
inline constexpr ImU32 kAxisText = IM_COL32(170, 175, 180, 255);
inline constexpr ImVec4 kHintGrey{0.5f, 0.5f, 0.5f, 1.0f};
inline constexpr float kAxisHeight = 18.0f;

using TimeToX = std::function<float(Time)>;  // ps from the Trigger → screen x

void DrawGrid(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float top,
              float bottom, ImU32 colour);
// Labels along an axis strip at `y`, each right of its line, clipped to [left, right].
void DrawAxisLabels(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float y,
                    float left, float right);
void DrawTrigger(ImDrawList& list, float x, float top, float bottom);  // line + triangle on top
// The Trigger off the view: an arrow at that end of the axis strip and its distance.
void DrawOffScreenTrigger(ImDrawList& list, Time distance, float left, float right, float y);
void DrawMarker(ImDrawList& list, float x, float top, float bottom);  // line + "M" tag on top
// One line: the cursor, marker and dt, or `hint` in grey when there are neither.
void StatusLine(std::optional<Time> cursor, std::optional<Time> marker, const char* hint);

// Whole wheel notches from fractional wheel input (trackpads): `rest` keeps the remainder.
int WheelNotches(float wheel, float& rest);

}  // namespace bw
