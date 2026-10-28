#include "machine/crack.hpp"

#include <catch2/catch_test_macros.hpp>

#include "board/copper.hpp"

using namespace bw;

namespace {

constexpr std::int64_t kMm = 1'000'000;

Point Mm(double x, double y) {
    return {static_cast<std::int64_t>(x * kMm), static_cast<std::int64_t>(y * kMm)};
}

Pad RoundPad(Point at) {
    return {.number = "1",
            .position = at,
            .angle = 0,
            .size = {1'600'000, 1'600'000},
            .cornerRadius = 0,
            .drill = 800'000,
            .shape = PadShape::Circle};
}

// Two round pads 10 mm apart on y = 0, joined by one front Trace.
Board Joined() {
    Board board;
    board.components.push_back({.reference = "U1", .pads = {RoundPad(Mm(0, 0))}});
    board.components.push_back({.reference = "U2", .pads = {RoundPad(Mm(10, 0))}});
    board.traces.push_back(
        {.layer = Layer::Front, .start = Mm(0, 0), .end = Mm(10, 0), .mid = {}, .width = 250'000});
    return board;
}

std::size_t IslandCount(const Board& board) { return FindIslands(board).islandSize.size(); }

}  // namespace

TEST_CASE("crack: splits the Trace into two islands", "[machine]") {
    const Board board = Joined();
    REQUIRE(IslandCount(board) == 1);
    const CrackDecl crack{1, Layer::Front, Mm(5, 0.1)};
    REQUIRE(FindCrackTrace(board, Layer::Front, crack.at) == 0u);
    const Board cracked = CrackBoard(board, {&crack, 1});
    CHECK(cracked.traces.size() == 2);
    CHECK(IslandCount(cracked) == 2);
}

TEST_CASE("crack: two cracks on one Trace", "[machine]") {
    const CrackDecl cracks[] = {{1, Layer::Front, Mm(3, 0)}, {2, Layer::Front, Mm(7, 0)}};
    const Board cracked = CrackBoard(Joined(), cracks);
    CHECK(cracked.traces.size() == 3);
    CHECK(IslandCount(cracked) == 3);
}

TEST_CASE("crack: off the Trace", "[machine]") {
    const Board board = Joined();
    CHECK(FindCrackTrace(board, Layer::Front, Mm(5, 0.2)).error() == "no Trace at F.Cu 5.00 0.20");
    CHECK(FindCrackTrace(board, Layer::Back, Mm(5, 0)).error() == "no Trace at B.Cu 5.00 0.00");
}

TEST_CASE("crack: arc Traces are refused", "[machine]") {
    Board board = Joined();
    board.traces[0].mid = Mm(5, 2);
    CHECK(FindCrackTrace(board, Layer::Front, Mm(2.5, 1)).error() ==
          "cracks on arc Traces are not supported");
}

TEST_CASE("crack: next to a pad does not cut", "[machine]") {
    const CrackDecl crack{1, Layer::Front, Mm(0.3, 0)};
    CHECK(IslandCount(CrackBoard(Joined(), {&crack, 1})) == 1);
}

TEST_CASE("crack: FormatMillimetres", "[machine]") {
    CHECK(FormatMillimetres(52'300'000) == "52.30");
    CHECK(FormatMillimetres(-1'250'000) == "-1.25");
    CHECK(FormatPoint(Layer::Back, Mm(1, 2)) == "B.Cu 1.00 2.00");
}
