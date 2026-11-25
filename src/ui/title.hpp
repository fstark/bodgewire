#pragma once

#include <cstddef>

#include <imgui.h>

namespace bw {

// The embedded Audiowide (fonts/README.md), for the launcher's title.
extern const unsigned char kTitleFont[];
extern const std::size_t kTitleFontSize;

// The launcher's title: BODGEWIRE in Audiowide, lit as a neon tube in cyan (the user's pick,
// 2026-11-22): a near-white core in a cyan glow. A failing sign now and then (NeonSignAt,
// 2026-11-25): a letter goes out, the supply dips; the time from ImGui's clock. `centreTop`: the
// middle of its top edge; `size`: the font's, px. Returns the drawn text's width.
float DrawTitle(ImDrawList& list, ImFont* font, ImVec2 centreTop, float size);

}  // namespace bw
