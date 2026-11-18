#pragma once

#include "ui/tv_view.hpp"
#include "view/tv_controls.hpp"

namespace bw {

// The TV window's name, for the shelf to bring it to the front.
inline constexpr const char* kTvWindowName = "TV";

// The TV as an object on the bench (P6d): a monitor case with no title bar, dragged by its body,
// resized from its corner keeping its proportions, put away by the × on the case. Drawn in the
// look of a small 1970s monitor (P6f): a cream shell, a charcoal front, the screen high, a strip
// of knobs (H-POS, V-POS, BRIGHT, CONTRAST), a pilot lamp and the monitor's own power switch, and
// an unlabelled underscan trimmer below the strip.
class TvWindow {
public:
    // Every frame, out or away: the tone first (a --tv tone shows on the first picture).
    // `gold`: the Machine has the keys (P6g). True when the case was clicked (not its ×).
    bool draw(bool& open, TvView& view, bool gold);
    TvControls& controls() { return controls_; }  // --tv

private:
    TvControls controls_;
};

}  // namespace bw
