#pragma once

#include <cstdint>

#include "view/board_scene.hpp"

namespace bw {

// The flip corner (P6o): a dog-ear on the Board's bottom-right corner that flips the Board (the
// Active Layer), whatever the tool. 4 mm along each edge: the Typewriter's SW1.2 is 4.5 mm in
// (its pad's corner, x and y summed), Tennis's J4.4 5.5 mm; never smaller than kFlipCornerMinPx
// on screen, where it may cover them (zoomed out: zoom in to reach them).
inline constexpr std::int64_t kFlipCornerSide = 4'000'000;  // nm
inline constexpr double kFlipCornerMinPx = 24;

// The dog-ear's side in nm at `pixelsPerMm`: kFlipCornerSide, at least kFlipCornerMinPx.
std::int64_t FlipCornerSide(double pixelsPerMm);
// The dog-ear: the triangle cut off `bounds`' bottom-right corner (Board y down), `side` along
// each edge. True when `at` is on it, its edges included.
bool OnFlipCorner(Point at, Bounds bounds, std::int64_t side);

}  // namespace bw
