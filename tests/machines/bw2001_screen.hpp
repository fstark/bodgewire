#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "devices/tv.hpp"

namespace bw {

// The BW-2001's 40 x 25 text window on the TV (P5q): a frame starts at VERT DRIVE's fall, a line
// at HORZ DRIVE's rise; the text's first dot is on TV row 41, column 144, one TV column a dot.
inline constexpr int kBw2001TextTop = 41, kBw2001TextLeft = 144;
inline constexpr int kBw2001Columns = 40, kBw2001Rows = 25;

// The character ROM 901447-08 (machines/130-bw2001/roms/901447-08.bin), 2 KB.
std::span<const std::uint8_t> Bw2001CharacterRom();

// The 8 lines of the 8 x 8 dots of cell `cell` (row * 40 + column) as drawn, bit 7 the leftmost.
std::array<std::uint8_t, 8> Bw2001CellDots(const TvFrame& frame, int cell);

// What screen code `code` looks like in the first character set (GRAPHIC low): the ROM's glyph,
// inverted when bit 7 is set.
std::array<std::uint8_t, 8> Bw2001Glyph(std::uint8_t code);

// Lit dots outside the text window (none on a working Machine).
int Bw2001BorderDots(const TvFrame& frame);

// The text on the TV as typist.rows() gives it from the video RAM: screen codes $00-$3F as
// BASIC's characters ($00-$1F are '@'-'_'), the reverse-video ones as their plain character,
// each row's trailing spaces and the trailing empty rows dropped; '?'+ a cell no text glyph.
std::vector<std::string> Bw2001TvRows(const TvFrame& frame);

}  // namespace bw
