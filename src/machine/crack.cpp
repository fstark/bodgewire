#include "machine/crack.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>

#include "sim/check.hpp"

namespace bw {

namespace {

constexpr double kCrackMargin = 50'000;  // nm each side beyond the Trace's half width

struct Vec {
    double x;
    double y;
};

Vec ToVec(Point p) { return {static_cast<double>(p.x), static_cast<double>(p.y)}; }
Point ToPoint(Vec v) { return {std::llround(v.x), std::llround(v.y)}; }

// Parameter t in [0, 1] of the point of segment a-b nearest to p.
double NearestT(Vec p, Vec a, Vec b) {
    const Vec ab{b.x - a.x, b.y - a.y};
    const double length2 = ab.x * ab.x + ab.y * ab.y;
    if (length2 == 0) {
        return 0;
    }
    return std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / length2, 0.0, 1.0);
}

double Distance(Vec p, Vec a, Vec b) {
    const double t = NearestT(p, a, b);
    return std::hypot(p.x - (a.x + (b.x - a.x) * t), p.y - (a.y + (b.y - a.y) * t));
}

}  // namespace

std::expected<std::size_t, std::string> FindCrackTrace(const Board& board, Layer layer, Point at) {
    const Vec p = ToVec(at);
    std::optional<std::size_t> best;
    double bestDistance = INFINITY;
    for (std::size_t i = 0; i < board.traces.size(); ++i) {
        const Trace& t = board.traces[i];
        if (t.layer != layer) {
            continue;
        }
        // An arc is approximated by its two chords through mid: enough to refuse it.
        const double d = t.mid ? std::min(Distance(p, ToVec(t.start), ToVec(*t.mid)),
                                          Distance(p, ToVec(*t.mid), ToVec(t.end)))
                               : Distance(p, ToVec(t.start), ToVec(t.end));
        if (d <= static_cast<double>(t.width) / 2 && d < bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    if (!best) {
        return std::unexpected("no Trace at " + FormatPoint(layer, at));
    }
    if (board.traces[*best].mid) {
        return std::unexpected(std::string("cracks on arc Traces are not supported"));
    }
    return *best;
}

Board CrackBoard(const Board& board, std::span<const CrackDecl> cracks) {
    Board result = board;
    for (const CrackDecl& crack : cracks) {
        const auto index = FindCrackTrace(result, crack.layer, crack.at);
        Check(index.has_value(), "crack: not on a straight Trace");
        const Trace t = result.traces[*index];
        result.traces.erase(result.traces.begin() + static_cast<std::ptrdiff_t>(*index));
        const Vec a = ToVec(t.start);
        const Vec b = ToVec(t.end);
        const double length = std::hypot(b.x - a.x, b.y - a.y);
        const double at = NearestT(ToVec(crack.at), a, b) * length;  // along the Trace
        const double gap = static_cast<double>(t.width) / 2 + kCrackMargin;
        const auto along = [&](double s) {
            return ToPoint({a.x + (b.x - a.x) * s / length, a.y + (b.y - a.y) * s / length});
        };
        if (at > gap) {
            result.traces.push_back({t.layer, t.start, along(at - gap), std::nullopt, t.width});
        }
        if (length - at > gap) {
            result.traces.push_back({t.layer, along(at + gap), t.end, std::nullopt, t.width});
        }
    }
    return result;
}

std::string FormatMillimetres(std::int64_t nm) {
    return std::format("{:.2f}", static_cast<double>(nm) / 1e6);
}

std::string FormatPoint(Layer layer, Point at) {
    return std::string(LayerName(layer)) + " " + FormatMillimetres(at.x) + " " +
           FormatMillimetres(at.y);
}

}  // namespace bw
