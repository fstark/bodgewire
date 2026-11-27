#pragma once

#include <cstdint>

#include <imgui.h>

namespace bw {

// The reading fonts (2026-11-27: players found the pages' text too small): the binder's pages and
// tabs and the Brief's note in Atkinson Hyperlegible, its Bold for titles and bold words, and
// Atkinson Hyperlegible Mono where characters line up in columns (code, a memory chip's
// contents). The rest of the UI keeps ImGui's own font.
//
// Running text is at the player's reading size (16, 20 or 24 px: the binder's aA key,
// view/reading_size.hpp); columns and controls whose places do not grow with it (a pinout, a
// memory chip's contents, the tabs) stay at kPageFontSize.
inline constexpr float kPageFontSize = 16;
enum class PageFace : std::uint8_t { Text, Bold, Mono };

// Once, before the first frame. Without it (a test), ImGui's font at the pages' size.
void LoadPageFonts(ImFontAtlas& fonts);
// The player's reading size in px, for the whole run: kPageFontSize until set.
void SetReadingSize(int px);
int ReadingSize();
// ImGui::PushFont of `face` at the reading size × `scale`; ImGui::PopFont() after.
void PushPageFont(PageFace face, float scale = 1);
// The same at kPageFontSize × `scale`, whatever the reading size.
void PushFixedPageFont(PageFace face, float scale = 1);

}  // namespace bw
