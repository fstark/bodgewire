#pragma once

#include <array>
#include <string>
#include <string_view>

namespace bw {

// The pages' reading sizes (2026-11-27): the running text of the binder's pages and of the Brief,
// in px. The binder's "aA" key steps through them, then back to the first; columns and controls
// (a pinout, a memory chip's contents, the tabs) keep the first.
inline constexpr std::array<int, 3> kReadingSizes{16, 20, 24};

// The size after `size`; after the last, or one not in the list, the first.
int NextReadingSize(int size);
// Whether `size` is one of kReadingSizes.
bool IsReadingSize(int size);

// The player's settings file (settings.txt beside the Progress): "# Bodgewire settings", then
// "text-size 20".
std::string SettingsText(int readingSize);
// Its text-size line's size; the first size when it has none, or one not in the list. Blank and
// '#' lines skipped. Never fails.
int ParseReadingSize(std::string_view settings);

}  // namespace bw
