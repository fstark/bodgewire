#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <imgui.h>

#include "sim/time.hpp"
#include "view/case_strip.hpp"

namespace bw {

// A drawn case's pieces (P8h's scope, P8i's analyzer), HP-like: a charcoal frame round a light
// grey panel, groups of light grey key caps on blue-grey panels, red LED windows, green lamps, the
// BW badge, the screen's dark bezel. Drawing only, on boxes offset by `at` (the window's top
// left); the window submits the items.

void DrawCaseFrame(ImDrawList& list, ImVec2 at, float width, float height);  // frame + panel
void DrawGroup(ImDrawList& list, ImVec2 at, const Box& group);  // a group's tinted panel
// A group's label, small capitals centred over [x0, x1] at `y`.
void DrawGroupLabel(ImDrawList& list, ImVec2 at, float x0, float x1, float y, const char* text);
void DrawBadge(ImDrawList& list, ImVec2 at, const Badge& badge);  // the logo, BODGE over WIRE
void DrawBezel(ImDrawList& list, ImVec2 at, const Box& bezel);
// A key cap with a text; pressed: lower, no shadow.
void DrawKey(ImDrawList& list, ImVec2 at, const Box& key, const char* label, bool pressed);
// A key cap lit from within (P8i: the analyzer's ARM, by its phase), `light` its colour; 0: dark,
// as DrawKey.
void DrawLitKey(ImDrawList& list, ImVec2 at, const Box& key, const char* label, bool pressed,
                ImU32 light);
// A ◀ or ▶ key, its arrow a triangle (the font has none).
void DrawArrowKey(ImDrawList& list, ImVec2 at, const Box& key, bool right, bool pressed);
// The size key: a key cap with the instruments' two-squares icon.
void DrawSizeKey(ImDrawList& list, ImVec2 at, const Box& key, bool pressed);
// A red LED window with its text centred.
void DrawLed(ImDrawList& list, ImVec2 at, const Box& window, const char* text);
// A green lamp, lit or dark, its name under it (TRIG'D, ARMED…).
void DrawLamp(ImDrawList& list, ImVec2 at, const Box& lamp, bool lit, const char* name);
// A rocker of 2 or 3 halves side by side (AUTO | NORM, EARLY | MID | LATE), the `lit` one raised.
void DrawRocker(ImDrawList& list, ImVec2 at, std::span<const Box> halves,
                std::span<const char* const> names, std::size_t lit);

// The grid of a drawn LED's values by unit (the scope's timebase, P8h; the analyzer's sample
// period, P8i), shown once ImGui::OpenPopup(id) was called, its top left at `below`: the value
// clicked, once. `values` shortest first.
std::optional<Time> TimeGrid(const char* id, std::span<const Time> values, Time current,
                             ImVec2 below);

// The LED window's look for a NumberField in it.
inline constexpr ImU32 kLedFrame = IM_COL32(26, 7, 5, 255);
inline constexpr ImU32 kLedFrameHovered = IM_COL32(44, 12, 9, 255);
inline constexpr ImU32 kLedText = IM_COL32(255, 74, 58, 255);
inline constexpr ImU32 kCasePanel = IM_COL32(222, 219, 210, 255);
inline constexpr ImU32 kCaseFrame = IM_COL32(60, 62, 66, 255);  // the window's background too

}  // namespace bw
