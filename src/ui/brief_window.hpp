#pragma once

#include <optional>

#include <imgui.h>

#include "ui/loaded.hpp"

namespace bw {

// The brief's window ID, for the shelf to bring it to the front.
inline constexpr const char* kBriefWindowName = "###brief";

inline constexpr float kNoteStrip = 16;  // the glued band along the top, a shade darker
inline constexpr ImVec4 kNoteInk{0.12f, 0.11f, 0.09f, 1.0f};

// The post-it's paper (P6o's brief) from `at` to `end`: the shadow under the curled corner, the
// paper with that corner cut, the glued strip, the curl; with `lift` px, the whole note's shadow
// that far below and right (the launcher's selected post-it, P7b).
void DrawNote(ImDrawList& list, ImVec2 at, ImVec2 end, float lift = 0);

// Where the note sits on the Board (P6o): its offset from the Board's top-left corner in screen
// px, moved by dragging the note, kept for the bench (folded away too).
struct BriefNote {
    ImVec2 offset{10, 10};
    bool dragging = false;
};

// The Puzzle's brief as a post-it stuck on the Board near its top-left corner (`corner`, the
// Board's on screen: it moves with the Board), as if the owner had slapped it there; dragged
// elsewhere on it; its × folds it into the shelf. On the bench's top centre when there is no
// Board. Nothing drawn without a brief.
void DrawBrief(bool& open, const Loaded& loaded, std::optional<ImVec2> corner, BriefNote& note);

}  // namespace bw
