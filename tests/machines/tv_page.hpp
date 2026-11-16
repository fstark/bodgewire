#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "devices/tv.hpp"

namespace bw {

// A page of character cells as the TV should show it (P5a's, shared since P5v): [line][dot],
// true = ink (black). Cells of 8 x 8 dots at 6.5 MHz, one line a TV row.
using PageDots = std::vector<std::vector<bool>>;

// `glyphs`: 64 glyphs of 8 rows, top row first, bit 7 the leftmost dot (a ZX80-style character
// set); `code(row, column)`: the cell's code, bit 7 inverting, bit 6 ignored.
PageDots ExpectedPage(std::span<const std::uint8_t> glyphs, int columns, int rows,
                      const std::function<unsigned(int row, int column)>& code);

// The page at (left, top), `left` in TV columns (a dot is not a whole number of columns),
// comparing each cell's dots 1-5 at each dot's centre.
bool Matches(const TvFrame& frame, const PageDots& page, double left, int top);

// Where the page is in `frame`, if anywhere: left in steps of a quarter column.
std::optional<std::pair<double, int>> FindPage(const TvFrame& frame, const PageDots& page);

// Ink (black) points of `frame`.
int InkCount(const TvFrame& frame);

}  // namespace bw
