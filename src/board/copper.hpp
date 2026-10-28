#pragma once

#include <cstdint>
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

// Point on one item's copper, with the shapes FindIslands uses.
bool Covers(const Pad& pad, Point at);
bool Covers(const Via& via, Point at);
bool Covers(const Trace& trace, Point at);

std::vector<Point> ArcPolyline(Point start, Point mid, Point end);  // FindIslands' chords

}  // namespace bw
