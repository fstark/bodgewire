#include "board/copper.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numbers>
#include <numeric>
#include <optional>
#include <vector>

#include "sim/check.hpp"

namespace bw {

namespace {

constexpr std::uint8_t kFront = 1;
constexpr std::uint8_t kBack = 2;
constexpr std::uint8_t kBothLayers = kFront | kBack;
constexpr int kArcChords = 16;

struct Vec {
    double x;
    double y;
};

Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y}; }
Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y}; }
Vec operator*(Vec a, double s) { return {a.x * s, a.y * s}; }
double Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
double Cross(Vec a, Vec b) { return a.x * b.y - a.y * b.x; }
Vec ToVec(Point p) { return {static_cast<double>(p.x), static_cast<double>(p.y)}; }

// KiCad: y down, positive angle counter-clockwise on screen.
Vec Rotate(Vec v, double degrees) {
    const double radians = degrees * std::numbers::pi / 180;
    const double cos = std::cos(radians);
    const double sin = std::sin(radians);
    return {v.x * cos + v.y * sin, -v.x * sin + v.y * cos};
}

// Convex hull of 1, 2 or 4 points, grown by `radius`.
struct CopperShape {
    std::uint32_t item;
    std::uint8_t layers;
    std::array<Vec, 4> hull;
    std::uint8_t hullSize;
    double radius;
    Vec min;
    Vec max;
};

double PointSegmentDistance(Vec p, Vec a, Vec b) {
    const Vec ab = b - a;
    const double length2 = Dot(ab, ab);
    const double t = length2 == 0 ? 0 : std::clamp(Dot(p - a, ab) / length2, 0.0, 1.0);
    const Vec d = p - (a + ab * t);
    return std::sqrt(Dot(d, d));
}

double SegmentDistance(Vec a, Vec b, Vec c, Vec d) {
    const double d1 = Cross(b - a, c - a);
    const double d2 = Cross(b - a, d - a);
    const double d3 = Cross(d - c, a - c);
    const double d4 = Cross(d - c, b - c);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
        return 0;
    }
    return std::min({PointSegmentDistance(a, c, d), PointSegmentDistance(b, c, d),
                     PointSegmentDistance(c, a, b), PointSegmentDistance(d, a, b)});
}

bool Inside(Vec p, const CopperShape& shape) {
    if (shape.hullSize < 3) {
        return false;
    }
    bool positive = false;
    bool negative = false;
    for (std::uint8_t i = 0; i < shape.hullSize; ++i) {
        const Vec a = shape.hull[i];
        const Vec b = shape.hull[(i + 1) % shape.hullSize];
        const double cross = Cross(b - a, p - a);
        positive = positive || cross > 0;
        negative = negative || cross < 0;
    }
    return !(positive && negative);
}

double HullDistance(const CopperShape& a, const CopperShape& b) {
    if (Inside(a.hull[0], b) || Inside(b.hull[0], a)) {
        return 0;
    }
    const auto edges = [](const CopperShape& s) { return s.hullSize == 4 ? 4 : 1; };
    double best = INFINITY;
    for (int i = 0; i < edges(a); ++i) {
        const Vec a0 = a.hull[i];
        const Vec a1 = a.hull[(i + 1) % a.hullSize];
        for (int j = 0; j < edges(b); ++j) {
            best = std::min(best, SegmentDistance(a0, a1, b.hull[j], b.hull[(j + 1) % b.hullSize]));
        }
    }
    return best;
}

// Chord end points of the arc start → mid → end; collinear → straight.
std::vector<Vec> ArcPoints(Vec start, Vec mid, Vec end) {
    const Vec b = mid - start;
    const Vec c = end - start;
    const double d = 2 * Cross(b, c);
    if (std::abs(d) < 1e-9 * Dot(c, c)) {
        return {start, end};
    }
    const Vec centre = start + Vec{(c.y * Dot(b, b) - b.y * Dot(c, c)) / d,
                                   (b.x * Dot(c, c) - c.x * Dot(b, b)) / d};
    const auto angleOf = [&](Vec p) { return std::atan2(p.y - centre.y, p.x - centre.x); };
    const auto positive = [](double a) {
        const double turn = 2 * std::numbers::pi;
        return std::fmod(std::fmod(a, turn) + turn, turn);
    };
    const double a0 = angleOf(start);
    double sweep = positive(angleOf(end) - a0);
    if (positive(angleOf(mid) - a0) > sweep) {
        sweep -= 2 * std::numbers::pi;
    }
    const Vec r = start - centre;
    const double radius = std::sqrt(Dot(r, r));
    std::vector<Vec> points{start};
    for (int i = 1; i < kArcChords; ++i) {
        const double a = a0 + sweep * i / kArcChords;
        points.push_back(centre + Vec{std::cos(a), std::sin(a)} * radius);
    }
    points.push_back(end);
    return points;
}

class ShapeBuilder {
public:
    std::vector<CopperShape> shapes;

    void add(std::uint32_t item, std::uint8_t layers, std::initializer_list<Vec> hull,
             double radius) {
        CopperShape shape{.item = item, .layers = layers, .hullSize = 0, .radius = radius};
        shape.min = {INFINITY, INFINITY};
        shape.max = {-INFINITY, -INFINITY};
        for (const Vec p : hull) {
            shape.hull[shape.hullSize++] = p;
            shape.min = {std::min(shape.min.x, p.x - radius), std::min(shape.min.y, p.y - radius)};
            shape.max = {std::max(shape.max.x, p.x + radius), std::max(shape.max.y, p.y + radius)};
        }
        shapes.push_back(shape);
    }

    void addPad(std::uint32_t item, const Pad& pad) {
        const Vec centre = ToVec(pad.position);
        const double w = static_cast<double>(pad.size.x);
        const double h = static_cast<double>(pad.size.y);
        const auto at = [&](double x, double y) { return centre + Rotate({x, y}, pad.angle); };
        switch (pad.shape) {
        case PadShape::Circle:
            add(item, kBothLayers, {centre}, w / 2);
            return;
        case PadShape::Oval: {
            const double half = std::abs(w - h) / 2;
            const Vec axis = w > h ? Vec{half, 0} : Vec{0, half};
            add(item, kBothLayers, {at(-axis.x, -axis.y), at(axis.x, axis.y)}, std::min(w, h) / 2);
            return;
        }
        case PadShape::Rectangle:
        case PadShape::RoundedRectangle: {
            const double r =
                pad.shape == PadShape::Rectangle ? 0 : static_cast<double>(pad.cornerRadius);
            const double x = w / 2 - r;
            const double y = h / 2 - r;
            add(item, kBothLayers, {at(-x, -y), at(x, -y), at(x, y), at(-x, y)}, r);
            return;
        }
        }
    }

    void addTrace(std::uint32_t item, const Trace& trace) {
        const std::uint8_t layer = trace.layer == Layer::Front ? kFront : kBack;
        const double radius = static_cast<double>(trace.width) / 2;
        const Vec start = ToVec(trace.start);
        const Vec end = ToVec(trace.end);
        if (!trace.mid) {
            add(item, layer, {start, end}, radius);
            return;
        }
        const Vec mid = ToVec(*trace.mid);
        const auto points = ArcPoints(start, mid, end);
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
            add(item, layer, {points[i], points[i + 1]}, radius);
        }
    }
};

bool Covers(const ShapeBuilder& builder, Point at) {
    const Vec p = ToVec(at);
    for (const CopperShape& shape : builder.shapes) {
        if (p.x < shape.min.x || p.x > shape.max.x || p.y < shape.min.y || p.y > shape.max.y) {
            continue;
        }
        if (Inside(p, shape)) {
            return true;
        }
        const int edges = shape.hullSize < 3 ? 1 : shape.hullSize;
        for (int i = 0; i < edges; ++i) {
            const Vec a = shape.hull[i];
            const Vec b = shape.hull[(i + 1) % shape.hullSize];
            if (PointSegmentDistance(p, a, b) <= shape.radius) {
                return true;
            }
        }
    }
    return false;
}

std::uint32_t Root(std::vector<std::uint32_t>& parent, std::uint32_t i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

// Item index of the first pad covering `at`.
std::optional<std::uint32_t> PadItemAt(const Board& board, Point at) {
    std::uint32_t item = 0;
    for (const BoardComponent& component : board.components) {
        for (const Pad& pad : component.pads) {
            ShapeBuilder builder;
            builder.addPad(item, pad);
            if (Covers(builder, at)) {
                return item;
            }
            ++item;
        }
    }
    return std::nullopt;
}

}  // namespace

Islands FindIslands(const Board& board) {
    ShapeBuilder builder;
    std::uint32_t item = 0;
    for (const BoardComponent& component : board.components) {
        for (const Pad& pad : component.pads) {
            builder.addPad(item++, pad);
        }
    }
    for (const Via& via : board.vias) {
        builder.add(item++, kBothLayers, {ToVec(via.position)},
                    static_cast<double>(via.diameter) / 2);
    }
    for (const Trace& trace : board.traces) {
        builder.addTrace(item++, trace);
    }

    std::vector<std::uint32_t> parent(item);
    std::iota(parent.begin(), parent.end(), 0);
    const auto& shapes = builder.shapes;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const CopperShape& a = shapes[i];
        for (std::size_t j = i + 1; j < shapes.size(); ++j) {
            const CopperShape& b = shapes[j];
            if ((a.layers & b.layers) == 0 || a.max.x < b.min.x || b.max.x < a.min.x ||
                a.max.y < b.min.y || b.max.y < a.min.y) {
                continue;
            }
            if (HullDistance(a, b) <= a.radius + b.radius) {
                parent[Root(parent, a.item)] = Root(parent, b.item);
            }
        }
    }
    for (const BodgeWire& wire : board.bodgeWires) {
        const auto a = PadItemAt(board, wire.a);
        const auto b = PadItemAt(board, wire.b);
        Check(a && b, "copper: Bodge Wire end off a pad");
        parent[Root(parent, *a)] = Root(parent, *b);
    }

    Islands islands;
    std::vector<std::uint32_t> islandOfRoot(item, UINT32_MAX);
    std::vector<std::uint32_t> islandOf(item);
    for (std::uint32_t i = 0; i < item; ++i) {
        std::uint32_t& island = islandOfRoot[Root(parent, i)];
        if (island == UINT32_MAX) {
            island = static_cast<std::uint32_t>(islands.islandSize.size());
            islands.islandSize.push_back(0);
        }
        ++islands.islandSize[island];
        islandOf[i] = island;
    }
    std::uint32_t next = 0;
    for (const BoardComponent& component : board.components) {
        auto& pads = islands.padIsland.emplace_back();
        for (std::size_t p = 0; p < component.pads.size(); ++p) {
            pads.push_back(islandOf[next++]);
        }
    }
    for (std::size_t v = 0; v < board.vias.size(); ++v) {
        islands.viaIsland.push_back(islandOf[next++]);
    }
    for (std::size_t t = 0; t < board.traces.size(); ++t) {
        islands.traceIsland.push_back(islandOf[next++]);
    }
    return islands;
}

bool Covers(const Pad& pad, Point at) {
    ShapeBuilder builder;
    builder.addPad(0, pad);
    return Covers(builder, at);
}

bool Covers(const Via& via, Point at) {
    ShapeBuilder builder;
    builder.add(0, kBothLayers, {ToVec(via.position)}, static_cast<double>(via.diameter) / 2);
    return Covers(builder, at);
}

bool Covers(const Trace& trace, Point at) {
    ShapeBuilder builder;
    builder.addTrace(0, trace);
    return Covers(builder, at);
}

std::vector<Point> ArcPolyline(Point start, Point mid, Point end) {
    std::vector<Point> result;
    for (const Vec v : ArcPoints(ToVec(start), ToVec(mid), ToVec(end))) {
        result.push_back({std::llround(v.x), std::llround(v.y)});
    }
    return result;
}

}  // namespace bw
