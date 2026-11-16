#pragma once

#include <cstdint>
#include <string_view>

#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/loaded.hpp"
#include "ui/tv_view.hpp"

namespace bw {

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

// The right column: Puzzle, TV, Knob, Chip, Repairs and Debug sections (TV and Knob when the
// shelf has them out). Routes pinout hits to the Board.
class SidePanel {
public:
    void draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
              DebugStats stats, std::uint64_t& seed, bool tvOut, bool knobOut);

private:
    ChipPanel chip_;
};

}  // namespace bw
