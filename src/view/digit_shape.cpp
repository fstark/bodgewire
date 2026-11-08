#include "view/digit_shape.hpp"

#include <array>
#include <cmath>
#include <numbers>

#include "sim/check.hpp"

namespace bw {

namespace {

// Digit units: u along right, v along up, origin the centre, unit the courtyard's length.
constexpr double kHeight = 0.62;      // character
constexpr double kWidth = 0.30;       // character
constexpr double kThick = 0.075;      // bar
constexpr double kShift = -0.04;      // character centre along u: room for the point
constexpr double kSlant = 0.08;       // u += kSlant · v
constexpr double kEndGap = 0.6;       // of kThick, each bar end pulled in
constexpr double kPointRadius = 0.6;  // of kThick

struct UV {
    double u, v;
};

// Hexagon from a to b, counter-clockwise in (u, v).
std::vector<UV> Bar(UV a, UV b) {
    const double length = std::hypot(b.u - a.u, b.v - a.v);
    const UV d{(b.u - a.u) / length, (b.v - a.v) / length};
    const UV n{-d.v, d.u};
    const double gap = kEndGap * kThick, half = kThick / 2;
    const UV p{a.u + d.u * gap, a.v + d.v * gap};
    const UV q{b.u - d.u * gap, b.v - d.v * gap};
    return {p,
            {p.u + d.u * half - n.u * half, p.v + d.v * half - n.v * half},
            {q.u - d.u * half - n.u * half, q.v - d.v * half - n.v * half},
            q,
            {q.u - d.u * half + n.u * half, q.v - d.v * half + n.v * half},
            {p.u + d.u * half + n.u * half, p.v + d.v * half + n.v * half}};
}

std::vector<UV> Octagon(UV centre, double radius) {
    std::vector<UV> points;
    for (int k = 0; k < 8; ++k) {
        const double angle = (k + 0.5) * std::numbers::pi / 4;
        points.push_back(
            {centre.u + radius * std::cos(angle), centre.v + radius * std::sin(angle)});
    }
    return points;
}

std::vector<UV> Outline(LampShape shape) {
    const double w = kWidth / 2, h = kHeight / 2;
    switch (shape) {
    case LampShape::SegmentA:
        return Bar({-w, h}, {w, h});
    case LampShape::SegmentB:
        return Bar({w, 0}, {w, h});
    case LampShape::SegmentC:
        return Bar({w, -h}, {w, 0});
    case LampShape::SegmentD:
        return Bar({-w, -h}, {w, -h});
    case LampShape::SegmentE:
        return Bar({-w, -h}, {-w, 0});
    case LampShape::SegmentF:
        return Bar({-w, 0}, {-w, h});
    case LampShape::SegmentG:
        return Bar({-w, 0}, {w, 0});
    case LampShape::DecimalPoint:
        return Octagon({w + 0.9 * kThick, -h}, kPointRadius * kThick);
    case LampShape::Round:
        break;
    }
    Check(false, "digit shape: a round lamp has no segment outline");
    return {};
}

}  // namespace

std::optional<DigitFrame> DigitFrameOf(std::span<const Point> courtyard, Point pad1) {
    if (courtyard.size() != 4) {
        return std::nullopt;
    }
    const auto dx = [&](std::size_t a, std::size_t b) {
        return static_cast<double>(courtyard[b].x - courtyard[a].x);
    };
    const auto dy = [&](std::size_t a, std::size_t b) {
        return static_cast<double>(courtyard[b].y - courtyard[a].y);
    };
    const double first = std::hypot(dx(0, 1), dy(0, 1));
    const double second = std::hypot(dx(1, 2), dy(1, 2));
    DigitFrame frame{};
    for (const Point& corner : courtyard) {
        frame.centreX += static_cast<double>(corner.x) / 4;
        frame.centreY += static_cast<double>(corner.y) / 4;
    }
    const bool firstLonger = first >= second;
    frame.length = firstLonger ? first : second;
    frame.breadth = firstLonger ? second : first;
    frame.upX = (firstLonger ? dx(0, 1) : dx(1, 2)) / frame.length;
    frame.upY = (firstLonger ? dy(0, 1) : dy(1, 2)) / frame.length;
    const double toCentre = (frame.centreX - static_cast<double>(pad1.x)) * frame.upX +
                            (frame.centreY - static_cast<double>(pad1.y)) * frame.upY;
    if (toCentre < 0) {
        frame.upX = -frame.upX;
        frame.upY = -frame.upY;
    }
    frame.rightX = -frame.upY;
    frame.rightY = frame.upX;
    return frame;
}

std::vector<Point> LampOutline(const DigitFrame& frame, LampShape shape) {
    std::vector<Point> points;
    for (const UV& p : Outline(shape)) {
        const double u = (p.u + kShift + kSlant * p.v) * frame.length;
        const double v = p.v * frame.length;
        points.push_back({std::llround(frame.centreX + u * frame.rightX + v * frame.upX),
                          std::llround(frame.centreY + u * frame.rightY + v * frame.upY)});
    }
    return points;
}

}  // namespace bw
