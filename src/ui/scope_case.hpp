#pragma once

#include <imgui.h>

#include "view/scope_layout.hpp"

namespace bw {

// The Oscilloscope's case (P8h), HP-like: a charcoal frame round a light grey panel, the strip's
// groups on blue-grey panels, light grey key caps, red LED windows, the BW logo. Drawing only, on
// ScopeCaseLayout's boxes offset by `at` (the window's top left); the window submits the items.

// The frame, the panel, the groups' panels and labels, the bezel and the badge.
void DrawScopeCase(ImDrawList& list, ImVec2 at, const ScopeCaseLayout& layout, float width,
                   float height);
// A key cap with a text; pressed: lower, no shadow.
void DrawKey(ImDrawList& list, ImVec2 at, const Box& key, const char* label, bool pressed);
// A ◀ or ▶ key, its arrow a triangle (the font has none).
void DrawArrowKey(ImDrawList& list, ImVec2 at, const Box& key, bool right, bool pressed);
// The size key: a key cap with the instruments' two-squares icon.
void DrawSizeKey(ImDrawList& list, ImVec2 at, const Box& key, bool pressed);
// A red LED window with its text centred.
void DrawLed(ImDrawList& list, ImVec2 at, const Box& window, const char* text);
// TRIG'D: green, lit or dark.
void DrawLamp(ImDrawList& list, ImVec2 at, const Box& lamp, bool lit);
// AUTO | NORM: the lit half raised.
void DrawRocker(ImDrawList& list, ImVec2 at, const Box& autoHalf, const Box& normHalf, bool normal);

// The LED window's look for the position's NumberField.
inline constexpr ImU32 kLedFrame = IM_COL32(26, 7, 5, 255);
inline constexpr ImU32 kLedFrameHovered = IM_COL32(44, 12, 9, 255);
inline constexpr ImU32 kLedText = IM_COL32(255, 74, 58, 255);
inline constexpr ImU32 kScopeCase = IM_COL32(222, 219, 210, 255);

}  // namespace bw
