#pragma once

#include <cstdint>
#include <string_view>

#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/loaded.hpp"
#include "ui/manual_view.hpp"
#include "ui/net_panel.hpp"
#include "ui/tv_view.hpp"

namespace bw {

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

// The right column: Puzzle, TV, Probe, Net, Chip, Repairs and Debug sections. Routes link,
// pinout and Net entry hits to the Board.
class SidePanel {
public:
    void draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
              DebugStats stats, std::uint64_t& seed);

private:
    ChipPanel chip_;
    NetPanel net_;
    ManualView puzzle_;  // the User Manual's `What it does`
};

}  // namespace bw
