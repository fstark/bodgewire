#pragma once

#include "ui/loaded.hpp"

namespace bw {

// The brief window's ID (its title is the Puzzle's), for the shelf to bring it to the front.
inline constexpr const char* kBriefWindowName = "###brief";

// The Puzzle's brief as a small yellow note on the bench (P6d; P6f makes it a post-it on the
// Board). Nothing drawn without a brief.
void DrawBrief(bool& open, const Loaded& loaded);

}  // namespace bw
