#pragma once

#include <imgui.h>

#include "view/keyboard_layout.hpp"

namespace bw {

// One cap in [a, b] (screen points), raised or pressed down; `u` is one key unit in points.
// Drawn as the ASCII keyboard's caps (P4o); also the Board's keys (P5i).
void DrawKeyCap(ImDrawList& list, ImFont* font, const KeyCap& cap, ImVec2 a, ImVec2 b, float u,
                bool down);

}  // namespace bw
