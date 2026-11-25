#pragma once

#include <optional>

#include <imgui.h>

#include "ui/tv_view.hpp"
#include "view/tv_controls.hpp"

namespace bw {

// The TV window's name, for the shelf to bring it to the front.
inline constexpr const char* kTvWindowName = "TV";

// The monitor's case (P6f) with its top-left at `at`, `width` wide (height width * kTvAspect): the
// cream shell filled and its edge, front, bezel, screen, strip, knobs at `controls`' turns, lamp,
// power switch, trimmer. The picture from `view` when `controls.on()` and `view` is not null,
// else the dark glass; `hoveredKnob` a knob drawn hovered. Draws only: no item, no input (the TV
// window's and the launcher's, P7b). `loupe`: the pointer, free over the TV's glass or pressed on
// it: where it is over the picture, a round loupe shows the pixels under it magnified, sharp,
// `loupeLift` px above it (a finger pressing there), and the mouse cursor is hidden.
void DrawMonitor(ImDrawList& list, ImVec2 at, float width, const TvControls& controls,
                 const TvView* view, std::optional<TvKnob> hoveredKnob = std::nullopt,
                 std::optional<ImVec2> loupe = std::nullopt, float loupeLift = 0);

// The TV as an object on the bench (P6d): a monitor case with no title bar, dragged by its body,
// resized from its corner keeping its proportions, put away by the × on the case. Drawn in the
// look of a small 1970s monitor (P6f): a cream shell, a charcoal front, the screen high, a strip
// of knobs (H-POS, V-POS, BRIGHT, CONTRAST), a pilot lamp and the monitor's own power switch, and
// an unlabelled underscan trimmer below the strip.
class TvWindow {
public:
    // Every frame, out or away: the tone first (a --tv tone shows on the first picture).
    void draw(bool& open, TvView& view);
    TvControls& controls() { return controls_; }  // --tv
    // --loupe: the loupe shown there on the screen whatever the mouse (screenshots).
    void setLoupe(std::optional<ScreenFraction> at) { loupeAt_ = at; }

private:
    TvControls controls_;
    std::optional<ScreenFraction> loupeAt_;
};

}  // namespace bw
