#include "board/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bw {

double Distance(Point a, Point b) {
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

double NearestT(Point p, Point a, Point b) {
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double length2 = dx * dx + dy * dy;
    if (length2 == 0) {
        return 0;
    }
    return std::clamp((static_cast<double>(p.x - a.x) * dx + static_cast<double>(p.y - a.y) * dy) /
                          length2,
                      0.0, 1.0);
}

Point NearestOnSegment(Point p, Point a, Point b) {
    const double t = NearestT(p, a, b);
    return {a.x + std::llround(static_cast<double>(b.x - a.x) * t),
            a.y + std::llround(static_cast<double>(b.y - a.y) * t)};
}

double SegmentDistance(Point p, Point a, Point b) {
    const double t = NearestT(p, a, b);
    return std::hypot(static_cast<double>(p.x - a.x) - static_cast<double>(b.x - a.x) * t,
                      static_cast<double>(p.y - a.y) - static_cast<double>(b.y - a.y) * t);
}

Point RotatedOffset(Point origin, double x, double y, double degrees) {
    const double radians = degrees * std::numbers::pi / 180;
    const double cos = std::cos(radians);
    const double sin = std::sin(radians);
    return {origin.x + std::llround(x * cos + y * sin),
            origin.y + std::llround(-x * sin + y * cos)};
}

}  // namespace bw
