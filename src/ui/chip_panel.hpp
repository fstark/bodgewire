#pragma once

#include <cstddef>
#include <optional>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"
#include "ui/manual_view.hpp"

namespace bw {

// The Chip section: header, role, pinout with Nets, Datasheet.
class ChipPanel {
public:
    // Hits: the title's reference, a pin's row or its name in the text hovered or clicked.
    TargetHits draw(const BoardScene& scene, std::size_t component, const Manuals& manuals,
                    const LoadedCircuit& design);

private:
    // The pin this frame's hits hover, for the next frame's group tint (P8j).
    void remember(const TargetHits& hits, std::size_t component);

    ManualView function_, pins_;
    std::optional<std::size_t> shown_;       // the component drawn last
    std::optional<std::size_t> hoveredPad_;  // its pin hovered last frame
};

}  // namespace bw
