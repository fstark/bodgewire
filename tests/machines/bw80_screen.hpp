#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "netlist/builder.hpp"
#include "tv_page.hpp"

namespace bw {

// The BW-80's RAM byte at `address` ($4000-$43FF, mirrored), read out of IC3 and IC4 (the ZX80's
// bit split: IC3 holds bits 7, 1, 0, 2 as IO1-IO4, IC4 bits 4, 3, 5, 6).
std::uint8_t Bw80Ram(const LoadedCircuit& loaded, std::uint16_t address);

// The display file (D_FILE at $400C: a $76, then rows of codes each ended by $76) as text, one
// string a row, trailing spaces kept out: the ZX80's codes as ASCII (space, " £ $ : ? ( ) - + *
// / = > < ; , . 0-9 A-Z), inverse letters as lower case, a graphic as '#', a token as '<n>'.
std::vector<std::string> Bw80DisplayRows(const LoadedCircuit& loaded);

// The display file as the TV should show it: 24 rows of 32 cells over `glyphs` (the ROM's
// character set), short rows padded with spaces.
PageDots Bw80ExpectedPage(const LoadedCircuit& loaded, std::span<const std::uint8_t> glyphs);

inline constexpr int kBw80Rows = 24, kBw80Columns = 32;

}  // namespace bw
