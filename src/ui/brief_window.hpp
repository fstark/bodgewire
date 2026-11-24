#pragma once

#include <optional>

#include <imgui.h>

#include "ui/loaded.hpp"

namespace bw {

// The brief's window ID, for the shelf to bring it to the front.
inline constexpr const char* kBriefWindowName = "###brief";

inline constexpr float kNoteStrip = 16;  // the glued band along the top, a shade darker
inline constexpr ImVec4 kNoteInk{0.12f, 0.11f, 0.09f, 1.0f};
inline constexpr ImU32 kBallpoint = IM_COL32(35, 60, 150, 255);  // the owner's pen (P8c)

// The owner's white index card: the Launcher's "Start the Machine" (P7b), the Thank-you Card (P8c).
inline constexpr ImU32 kCardPaper = IM_COL32(244, 242, 235, 255);
inline constexpr ImU32 kCardRed = IM_COL32(214, 96, 96, 255);     // its rule under the title
inline constexpr ImU32 kCardBlue = IM_COL32(150, 180, 214, 255);  // its lines
inline constexpr ImU32 kCardShadow = IM_COL32(0, 0, 0, 110);

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
// Board. Nothing drawn without a brief. `ticked` (P8c): the Puzzle is completed, the owner's
// tick and thanks under the brief.
void DrawBrief(bool& open, const OpenedMachine& loaded, std::optional<ImVec2> corner,
               BriefNote& note, bool ticked = false);

}  // namespace bw
