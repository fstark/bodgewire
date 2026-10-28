#pragma once

#include <optional>

#include "manual/manual.hpp"
#include "netlist/net_pins.hpp"
#include "ui/board_view.hpp"

namespace bw {

struct NetPanelHits {
    TargetHits targets;  // entry hovered / clicked
    bool clear = false;  // `Clear` pressed
};

// The Net section: header, Drivers, Loads, each entry a link to its pad.
class NetPanel {
public:
    NetPanelHits draw(const BoardScene& scene, NetId net, const LoadedCircuit& design,
                      const Manuals& manuals);

private:
    std::optional<NetId> cached_;  // design is fixed for the run
    NetPins pins_;
};

}  // namespace bw
