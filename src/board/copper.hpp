#pragma once

#include <cstdint>
#include <vector>

#include "board/board.hpp"

namespace bw {

// Connected groups of copper (pads, vias, Traces), from geometry only.
struct Islands {
    std::vector<std::vector<std::uint32_t>> padIsland;  // [component][pad] → island
    std::vector<std::uint32_t> islandSize;              // copper items per island
};

Islands FindIslands(const Board& board);

}  // namespace bw
