#pragma once

// The use look of the Board (P6 prototype): the Board as the real thing, parts drawn as the
// parts (chip bodies, resistors with their bands, the oscillator's can, connector housings),
// copper under green solder mask, references printed on the silkscreen beside each part.
// No pin names, outlines on hover or tooltips: those belong to bench mode.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "board/board.hpp"
#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

// Board nm to screen pixels, as BoardView's own mapping.
struct LookMap {
    const Viewport& viewport;
    ImVec2 origin;

    ImVec2 operator()(Point p) const {
        const ScreenPoint s = viewport.toScreen(p);
        return {origin.x + static_cast<float>(s.x), origin.y + static_cast<float>(s.y)};
    }
    float pixels(std::int64_t nm) const { return static_cast<float>(viewport.toPixels(nm)); }
};

// The mask, the copper under it, the board edge: below the pads.
// `glowing`: Nets whose copper is drawn brighter, every other trace dimmed (the chip the player
// works on: where it is wired, on both layers); empty for the plain colours.
void DrawUseBase(ImDrawList& list, const LookMap& map, const BoardScene& scene, Layer active,
                 std::span<const NetId> glowing);
// Silkscreen outlines, then the parts' bodies and leads: above the pads. `own`: Components
// drawn by BoardView itself (lamps, digits, switches), which get silkscreen only. `xray`: the
// part the player works on, drawn see-through so its pins and their names show; `alpha`: every
// part's opacity (low with the back layer active: the Board seen from below).
void DrawUseParts(ImDrawList& list, const LookMap& map, const BoardScene& scene,
                  const std::vector<bool>& own, std::optional<std::size_t> xray, float alpha);
// The references, beside each part: above everything else on the Board.
void DrawUseReferences(ImDrawList& list, const LookMap& map, const BoardScene& scene);

}  // namespace bw
