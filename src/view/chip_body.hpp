#pragma once

// A chip's drawn epoxy body and a memory chip's window (P6p): shared by the real look's drawing
// (ui/board_look.cpp) and the window's click (ui/board_view.cpp). BoardScene::bodyAt (the
// courtyard) stays what a pick calls the body.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "board/board.hpp"
#include "view/board_scene.hpp"

namespace bw {

// A box on the Board, nm.
struct BodyBox {
    Point min, max;
    std::int64_t width() const { return max.x - min.x; }
    std::int64_t height() const { return max.y - min.y; }
    Point centre() const { return {(min.x + max.x) / 2, (min.y + max.y) / 2}; }
    BodyBox grown(std::int64_t by) const {
        return {{min.x - by, min.y - by}, {max.x + by, max.y + by}};
    }
};

// The box through a part's pad centres; nullopt without pads.
std::optional<BodyBox> PadCentres(const BoardComponent& component);

struct ChipBody {
    BodyBox box;
    bool horizontal;  // pad rows along x
};
// The epoxy body between a DIP's pad rows, past its end pins.
std::optional<ChipBody> BodyOf(const BoardComponent& component);

// A chip holding a program or a character set: an EPROM shows its quartz window, a mask ROM only
// while x-rayed (its die seen through the epoxy). None: no window, no contents.
enum class MemoryLook : std::uint8_t { None, Eprom, Mask };
MemoryLook MemoryLookOf(std::string_view part);  // 2716 Eprom; 2332, 2316B, 2513, 6530 Mask

// The window at the body's centre: the quartz's radius 30 % of the body's width, the seal's 23/20
// of it.
struct MemoryWindow {
    Point centre;
    std::int64_t quartz, seal;  // radii, nm
    bool horizontal;            // the body's
};
// nullopt: not a memory chip, or no body.
std::optional<MemoryWindow> MemoryWindowOf(const BoardComponent& component, std::string_view part);
// The chip whose window (its seal included) covers `at`, if any.
std::optional<std::size_t> MemoryWindowAt(const BoardScene& scene, Point at);

}  // namespace bw
