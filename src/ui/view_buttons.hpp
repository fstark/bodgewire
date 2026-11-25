#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <imgui.h>

namespace bw {

// The Board's view buttons (P8f): a column at the Board panel's bottom right, for what a
// double-click on bare Board (Fit), Tab (Look), Option held (Copper) and ` (Flip) do, and Hold
// for a right-click's latch. Flip is the lowest, by the Board's own Flip Corner.
enum class ViewButton : std::uint8_t { Fit, Look, Copper, Hold, Flip };

inline constexpr float kViewButton = 44;  // a button's side, px
inline constexpr float kViewButtonGap = 4;
inline constexpr float kViewButtonMargin = 4;  // from the panel's edges

struct ViewButtonsLook {
    bool plain = false;   // Look drawn on: the plain look
    bool copper = false;  // Copper drawn on
    bool hold = false;    // Hold drawn on
    bool back = false;    // Flip drawn on: the back side is the Active Layer
};

class ViewButtons {
public:
    // The buttons' items, before the Board's input (a press on them is not the Board's): `hold`
    // whether the Hold button is there (a Board with a button or a key). The button clicked.
    std::optional<ViewButton> items(ImVec2 panelMin, ImVec2 panelMax, bool hold);
    // The pointer is on the column (from the last items()).
    bool hovered() const { return hovered_; }
    // Over the Board: each button a dark plate with its drawing, framed in gold when on.
    void draw(ImDrawList& list, const ViewButtonsLook& look) const;

private:
    struct Placed {
        ViewButton button;
        ImVec2 min;
        bool hovered;
    };
    std::vector<Placed> placed_;
    bool hovered_ = false;
};

}  // namespace bw
