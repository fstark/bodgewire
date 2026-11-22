#include "view/output_frame.hpp"

#include <algorithm>
#include <optional>

namespace bw {

namespace {

double Area(Bounds b) {
    return static_cast<double>(b.max.x - b.min.x) * static_cast<double>(b.max.y - b.min.y);
}

}  // namespace

Bounds OutputFrame(const BoardScene& scene, bool hasTv) {
    const Bounds board = scene.bounds();
    if (hasTv || (board.max.x - board.min.x <= kSmallBoardSide &&
                  board.max.y - board.min.y <= kSmallBoardSide)) {
        return board;
    }
    std::optional<Bounds> lamps;
    for (std::size_t k = 0; k < scene.board().components.size(); ++k) {
        if (!scene.lamp(k)) {
            continue;
        }
        for (const Point p : scene.courtyard(k)) {
            if (!lamps) {
                lamps = Bounds{p, p};
            }
            lamps->min = {std::min(lamps->min.x, p.x), std::min(lamps->min.y, p.y)};
            lamps->max = {std::max(lamps->max.x, p.x), std::max(lamps->max.y, p.y)};
        }
    }
    if (!lamps) {
        return board;
    }
    const Bounds frame{{std::max(board.min.x, lamps->min.x - kFrameMargin),
                        std::max(board.min.y, lamps->min.y - kFrameMargin)},
                       {std::min(board.max.x, lamps->max.x + kFrameMargin),
                        std::min(board.max.y, lamps->max.y + kFrameMargin)}};
    return Area(frame) > kWholeShare * Area(board) ? board : frame;
}

}  // namespace bw
