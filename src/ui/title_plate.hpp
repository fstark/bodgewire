#pragma once

#include <imgui.h>

namespace bw {

// The launcher's title: BODGEWIRE drawn as copper traces on a small board, in the Board's own
// colours (its mask, its front traces, its tinned pads), a mounting hole in each corner, one
// trace of the I broken and mended by a bodge wire. `centreTop`: the middle of its top edge;
// `height`: the plate's, its width in proportion. Returns the width.
float DrawTitlePlate(ImDrawList& list, ImVec2 centreTop, float height);

}  // namespace bw
