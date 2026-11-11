#pragma once

#include <cstdint>
#include <string_view>

#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/keyboard_capture.hpp"
#include "ui/loaded.hpp"
#include "ui/tv_view.hpp"

namespace bw {

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

// The right column: Puzzle, TV, Knob, Keyboard, Probe, Chip, Repairs and Debug sections. Routes
// pinout hits to the Board.
class SidePanel {
public:
    void draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
              KeyboardCapture& capture, DebugStats stats, std::uint64_t& seed);

private:
    ChipPanel chip_;
};

}  // namespace bw
