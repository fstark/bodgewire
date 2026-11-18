#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "board/board.hpp"

namespace bw {

// A grease pencil's mark on the Board (P6h): the player's note, drawn whatever the tool or layer.
enum class MarkShape : std::uint8_t { Dot, Cross, Check };

struct PencilMark {
    Point at;  // nm, Board coordinates
    MarkShape shape;
    std::optional<std::size_t> body;  // the Component whose body it is on: gone with a swap
};

// The marks of a session, in the order put down (the later drawn on top).
class PencilMarks {
public:
    std::span<const PencilMark> marks() const { return marks_; }
    // A pencil click at `at`: the nearest mark within `reach` nm takes the next shape (dot,
    // cross, check, then none: removed); with none in reach, a dot there, on `body`.
    void click(Point at, std::optional<std::size_t> body, std::int64_t reach);
    // A Shift-click or right-click: the nearest mark within `reach` removed; false: none there.
    bool clear(Point at, std::int64_t reach);
    void swapped(std::size_t component);  // the marks on its body removed
    void add(PencilMark mark);            // --mark

private:
    std::optional<std::size_t> nearest(Point at, std::int64_t reach) const;

    std::vector<PencilMark> marks_;
};

}  // namespace bw
