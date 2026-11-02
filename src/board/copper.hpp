#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "board/board.hpp"

namespace bw {

// Connected groups of copper (pads, vias, Traces), from geometry only.
struct Islands {
    std::vector<std::vector<std::uint32_t>> padIsland;  // [component][pad] → island
    std::vector<std::uint32_t> viaIsland;               // [via] → island
    std::vector<std::uint32_t> traceIsland;             // [trace] → island
    std::vector<std::uint32_t> islandSize;              // copper items per island
};

Islands FindIslands(const Board& board);

// Copper items numbered as FindIslands does: every pad (component by component), then vias, then
// Traces. Trace t is item pads + vias + t.
struct CopperGraph {
    std::uint32_t pads = 0, vias = 0;
    std::vector<std::vector<std::uint32_t>> touching;  // [item] → touching items, ascending
};

CopperGraph BuildCopperGraph(const Board& board);  // Bodge Wires join their two pads
std::optional<std::uint32_t> PadItemAt(const Board& board, Point at);  // first pad covering `at`
// `piece` (a Trace not on the Board) touches item `item`'s copper, with FindIslands' shapes.
bool Touches(const Board& board, std::uint32_t item, const Trace& piece);

// Point on one item's copper, with the shapes FindIslands uses.
bool Covers(const Pad& pad, Point at);
bool Covers(const Via& via, Point at);
bool Covers(const Trace& trace, Point at);

std::vector<Point> ArcPolyline(Point start, Point mid, Point end);  // FindIslands' chords

}  // namespace bw
