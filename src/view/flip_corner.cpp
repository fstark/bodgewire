#include "view/flip_corner.hpp"

#include <algorithm>
#include <cmath>

namespace bw {

std::int64_t FlipCornerSide(double pixelsPerMm) {
    const auto floor =
        static_cast<std::int64_t>(std::llround(kFlipCornerMinPx / pixelsPerMm * 1e6));
    return std::max(kFlipCornerSide, floor);
}

bool OnFlipCorner(Point at, Bounds bounds, std::int64_t side) {
    if (at.x > bounds.max.x || at.y > bounds.max.y) {
        return false;
    }
    // Inside the corner's square and on the corner's side of its diagonal.
    return (at.x - (bounds.max.x - side)) + (at.y - (bounds.max.y - side)) >= side;
}

}  // namespace bw
