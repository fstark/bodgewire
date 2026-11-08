#pragma once

#include <optional>
#include <span>
#include <vector>

#include "board/board.hpp"
#include "parts/catalogue.hpp"

namespace bw {

// A digit's own axes on the Board: it stands along its courtyard's long side, bottom on the side
// of pad 1. nm; `up` and `right` unit vectors (Board y grows downwards).
struct DigitFrame {
    double centreX, centreY;
    double upX, upY, rightX, rightY;
    double length, breadth;  // courtyard long and short sides
};

// nullopt unless the courtyard has 4 corners.
std::optional<DigitFrame> DigitFrameOf(std::span<const Point> courtyard, Point pad1);

// Convex outline of one segment or the decimal point, Board nm, one winding for every rotation.
// Check()s shape != Round.
std::vector<Point> LampOutline(const DigitFrame& frame, LampShape shape);

}  // namespace bw
