#include "machine/crack.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <vector>

#include "board/copper.hpp"
#include "machine/puzzle.hpp"
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

constexpr double kCrackStep = 10'000;  // nm between tried crack positions

// A crackable run along one Trace.
struct Stretch {
    std::size_t trace;  // index into board.traces
    bool reversed;      // the path enters at Trace::end
    double from, to;    // nm along the Trace from its entry end; from <= to
};

// Fewest-items path from `from` to `to`; BFS with neighbours in ascending order.
std::optional<std::vector<std::uint32_t>> Path(const CopperGraph& graph, std::uint32_t from,
                                               std::uint32_t to) {
    constexpr std::uint32_t kNone = UINT32_MAX;
    std::vector<std::uint32_t> previous(graph.touching.size(), kNone);
    std::deque<std::uint32_t> queue{from};
    previous[from] = from;
    while (!queue.empty() && previous[to] == kNone) {
        const std::uint32_t item = queue.front();
        queue.pop_front();
        for (const std::uint32_t next : graph.touching[item]) {
            if (previous[next] == kNone) {
                previous[next] = item;
                queue.push_back(next);
            }
        }
    }
    if (previous[to] == kNone) {
        return std::nullopt;
    }
    std::vector<std::uint32_t> path{to};
    while (path.back() != from) {
        path.push_back(previous[path.back()]);
    }
    std::ranges::reverse(path);
    return path;
}

Vec Along(Vec entry, Vec exit, double length, double s) {
    return {entry.x + (exit.x - entry.x) * s / length, entry.y + (exit.y - entry.y) * s / length};
}

std::vector<Stretch> Stretches(const Board& board, const CopperGraph& graph,
                               std::span<const std::uint32_t> path, Point a) {
    std::vector<Stretch> stretches;
    Vec cursor = ToVec(a);
    const std::uint32_t firstTrace = graph.pads + graph.vias;
    for (const std::uint32_t item : path) {
        if (item < firstTrace) {
            continue;
        }
        const std::size_t index = item - firstTrace;
        const Trace& t = board.traces[index];
        const Vec start = ToVec(t.start);
        const Vec end = ToVec(t.end);
        const bool reversed = std::hypot(end.x - cursor.x, end.y - cursor.y) <
                              std::hypot(start.x - cursor.x, start.y - cursor.y);
        const Vec entry = reversed ? end : start;
        const Vec exit = reversed ? start : end;
        cursor = exit;
        if (t.mid) {
            continue;
        }
        const double length = std::hypot(exit.x - entry.x, exit.y - entry.y);
        const double gap = static_cast<double>(t.width) / 2 + kCrackMargin;
        std::optional<Stretch> run;
        for (double s = gap; s <= length - gap; s += kCrackStep) {
            const Trace piece{t.layer, ToPoint(Along(entry, exit, length, s - gap)),
                              ToPoint(Along(entry, exit, length, s + gap)), std::nullopt, t.width};
            const bool clear = std::ranges::none_of(graph.touching[item], [&](std::uint32_t other) {
                return Touches(board, other, piece);
            });
            if (clear) {
                if (!run) {
                    run = Stretch{index, reversed, s, s};
                }
                run->to = s;
            } else if (run) {
                stretches.push_back(*run);
                run.reset();
            }
        }
        if (run) {
            stretches.push_back(*run);
        }
    }
    return stretches;
}

}  // namespace

std::expected<Crack, std::string> PlaceCrack(const Board& board, Point a, Point b, double percent) {
    const auto ia = PadItemAt(board, a);
    const auto ib = PadItemAt(board, b);
    Check(ia && ib, "PlaceCrack: not a pad centre");
    const CopperGraph graph = BuildCopperGraph(board);
    const auto path = Path(graph, *ia, *ib);
    if (!path) {
        return std::unexpected(std::string("pads not joined by Copper"));
    }
    const std::vector<Stretch> stretches = Stretches(board, graph, *path, a);
    if (stretches.empty()) {
        return std::unexpected(std::string("no crackable Trace between the pads"));
    }
    double total = 0;
    for (const Stretch& stretch : stretches) {
        total += stretch.to - stretch.from;
    }
    double target = percent / 100 * total;
    const Stretch* chosen = nullptr;
    double s = 0;
    for (const Stretch& stretch : stretches) {
        const double length = stretch.to - stretch.from;
        if (target <= length || &stretch == &stretches.back()) {
            chosen = &stretch;
            s = stretch.from + std::min(target, length);
            break;
        }
        target -= length;
    }
    const Trace& t = board.traces[chosen->trace];
    const Vec entry = ToVec(chosen->reversed ? t.end : t.start);
    const Vec exit = ToVec(chosen->reversed ? t.start : t.end);
    const double length = std::hypot(exit.x - entry.x, exit.y - entry.y);
    const Crack crack{t.layer, ToPoint(Along(entry, exit, length, s))};
    if (Path(BuildCopperGraph(CrackBoard(board, {&crack, 1})), *ia, *ib)) {
        return std::unexpected(std::string("does not separate the pads"));
    }
    return crack;
}

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

Board CrackBoard(const Board& board, std::span<const Crack> cracks) {
    Board result = board;
    for (const Crack& crack : cracks) {
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
