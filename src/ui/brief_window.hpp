#pragma once

#include <optional>

#include <imgui.h>

#include "ui/loaded.hpp"

namespace bw {

// The brief's window ID, for the shelf to bring it to the front.
inline constexpr const char* kBriefWindowName = "###brief";

// The Puzzle's brief as a post-it stuck on the Board's top-left corner (`corner`, the Board's
// on screen: it moves with the Board), as if the owner had slapped it there; its × folds it into
// the shelf. On the bench's top centre when there is no Board. Nothing drawn without a brief.
void DrawBrief(bool& open, const Loaded& loaded, std::optional<ImVec2> corner);

}  // namespace bw
