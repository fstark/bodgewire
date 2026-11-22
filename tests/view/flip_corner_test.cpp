#include "view/flip_corner.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/kicad_board.hpp"

using namespace bw;

TEST_CASE("flip corner: 4 mm, never under its pixel floor", "[view]") {
    CHECK(FlipCornerSide(20) == kFlipCornerSide);  // 4 mm = 80 px
    CHECK(FlipCornerSide(2) == 12'000'000);        // 24 px at 2 px/mm: 12 mm
}

TEST_CASE("flip corner: the triangle at the bottom-right corner", "[view]") {
    const Bounds board{{0, 0}, {100'000'000, 60'000'000}};
    constexpr std::int64_t kSide = 8'000'000;
    CHECK(OnFlipCorner(board.max, board, kSide));  // the corner
    CHECK(
        OnFlipCorner({100'000'000 - 4'000'000, 60'000'000 - 4'000'000}, board, kSide));  // diagonal
    CHECK(OnFlipCorner({100'000'000 - 1'000'000, 60'000'000 - 6'000'000}, board, kSide));
    CHECK_FALSE(OnFlipCorner({100'000'000 - 5'000'000, 60'000'000 - 5'000'000}, board, kSide));
    CHECK_FALSE(OnFlipCorner({100'000'000 + 1, 60'000'000}, board, kSide));  // outside the Board
    CHECK_FALSE(OnFlipCorner({0, 0}, board, kSide));                         // the opposite corner
    CHECK_FALSE(OnFlipCorner({100'000'000 - 9'000'000, 60'000'000}, board, kSide));  // past it
}

TEST_CASE("flip corner: no pad of any Machine's Board under it", "[view]") {
    int boards = 0;
    for (const auto& machine : std::filesystem::directory_iterator(test::RootDir() / "machines")) {
        if (!std::filesystem::is_directory(machine.path() / "board")) {
            continue;
        }
        for (const auto& file : std::filesystem::directory_iterator(machine.path() / "board")) {
            if (file.path().extension() != ".kicad_pcb") {
                continue;
            }
            const auto board = ReadKicadBoardFile(file.path());
            REQUIRE(board.has_value());
            ++boards;
            Bounds bounds{{std::numeric_limits<std::int64_t>::max(),
                           std::numeric_limits<std::int64_t>::max()},
                          {std::numeric_limits<std::int64_t>::min(),
                           std::numeric_limits<std::int64_t>::min()}};
            for (const OutlineEdge& edge : board->outline) {
                for (const Point p : {edge.start, edge.end}) {
                    bounds.min = {std::min(bounds.min.x, p.x), std::min(bounds.min.y, p.y)};
                    bounds.max = {std::max(bounds.max.x, p.x), std::max(bounds.max.y, p.y)};
                }
            }
            for (const BoardComponent& component : board->components) {
                for (const Pad& pad : component.pads) {
                    // The pad's corner nearest the Board's (its larger half-side both ways).
                    const std::int64_t half = std::max(pad.size.x, pad.size.y) / 2;
                    const Point near{pad.position.x + half, pad.position.y + half};
                    INFO(file.path().filename().string()
                         << " " << component.reference << "." << pad.number);
                    CHECK_FALSE(OnFlipCorner(near, bounds, kFlipCornerSide));
                }
            }
        }
    }
    CHECK(boards >= 14);
}
