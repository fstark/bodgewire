#pragma once

#include "ui/tv_view.hpp"

namespace bw {

// The TV window's name, for the shelf to bring it to the front.
inline constexpr const char* kTvWindowName = "TV";

// The TV as an object on the bench (P6d): a monitor case with no title bar, dragged by its body,
// resized from its corner keeping its proportions, put away by the × on the case.
class TvWindow {
public:
    void draw(bool& open, const TvView& view, bool powered);
};

}  // namespace bw
