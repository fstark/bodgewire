#pragma once

#include <optional>

#include "devices/tv.hpp"

namespace bw {

// The lit part (cells >= 0.5) of `frame`'s rows [fromRow, toRow): rows and columns, inclusive;
// nullopt if nothing is lit there (P6n).
struct LitBox {
    int top, bottom, left, right;
};
std::optional<LitBox> LitBounds(const TvFrame& frame, int fromRow = 0, int toRow = kTvRows);

// `box` inside a real monitor's window (P6f's kWindowLeft, kWindowRight, kWindowTop; the bottom
// row is `lines` + 1, `lines` the frame's line starts, 312 on the early TV Machines).
bool InsideWindow(const LitBox& box, int lines);

}  // namespace bw
