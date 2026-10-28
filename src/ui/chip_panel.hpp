#pragma once

#include <cstddef>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"
#include "ui/manual_view.hpp"

namespace bw {

// The Chip section: header, role, pinout with Nets, Datasheet.
class ChipPanel {
public:
    // Hits: pinout pin hovered/clicked, `Zoom to` → clicked {component}.
    TargetHits draw(const BoardScene& scene, std::size_t component, const Manuals& manuals,
                    const LoadedCircuit& design);

private:
    ManualView function_, pins_;
};

}  // namespace bw
