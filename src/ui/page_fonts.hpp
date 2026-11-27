#pragma once

#include <cstdint>

#include <imgui.h>

namespace bw {

// The reading fonts (2026-11-27: players found the pages' text too small): the binder's pages and
// tabs and the Brief's note in Atkinson Hyperlegible at 16 px, its Bold for titles and bold words,
// and Atkinson Hyperlegible Mono where characters line up in columns (code, a memory chip's
// contents). The rest of the UI keeps ImGui's own font.
inline constexpr float kPageFontSize = 16;
enum class PageFace : std::uint8_t { Text, Bold, Mono };

// Once, before the first frame. Without it (a test), ImGui's font at the pages' size.
void LoadPageFonts(ImFontAtlas& fonts);
// ImGui::PushFont of `face` at kPageFontSize × `scale`; ImGui::PopFont() after.
void PushPageFont(PageFace face, float scale = 1);

}  // namespace bw
