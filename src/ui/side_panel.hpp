#pragma once

#include <cstdint>
#include <string_view>

#include "ui/board_view.hpp"
#include "ui/loaded.hpp"
#include "ui/tv_view.hpp"

namespace bw {

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

// The right column: Puzzle, TV, Probe, Chip, Repairs and Debug sections.
class SidePanel {
public:
    void draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
              DebugStats stats, std::uint64_t& seed);
};

}  // namespace bw
