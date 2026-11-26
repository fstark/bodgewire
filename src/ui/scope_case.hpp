#pragma once

#include <imgui.h>

#include "ui/case_keys.hpp"
#include "view/scope_layout.hpp"

namespace bw {

// The Oscilloscope's case (P8h): the frame, the panel, the strip's groups and labels, the badge
// and the bezel, from case_keys on ScopeCaseLayout's boxes offset by `at` (the window's top
// left); the window submits the items and draws the keys.
void DrawScopeCase(ImDrawList& list, ImVec2 at, const ScopeCaseLayout& layout, float width,
                   float height);

}  // namespace bw
