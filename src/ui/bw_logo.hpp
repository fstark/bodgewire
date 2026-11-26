#pragma once

#include <imgui.h>

namespace bw {

// The Bodge Wire logo (P8h), a spoof of HP's: a blue rounded rectangle 1000 × 622 units, a white
// circle, `bw` in slanted straight strokes inside it, the b's stem leaving through the top edge
// with a white gap each side. Drawn `height` px high at `min` (1000 / 622 as wide).
void DrawBwLogo(ImDrawList& list, ImVec2 min, float height);

}  // namespace bw
