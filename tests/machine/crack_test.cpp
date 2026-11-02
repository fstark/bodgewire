#include "machine/crack.hpp"

#include <cmath>
#include <span>

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
    const Crack crack{Layer::Front, Mm(5, 0.1)};
    REQUIRE(FindCrackTrace(board, Layer::Front, crack.at) == 0u);
    const Board cracked = CrackBoard(board, {&crack, 1});
    CHECK(cracked.traces.size() == 2);
    CHECK(IslandCount(cracked) == 2);
}

TEST_CASE("crack: two cracks on one Trace", "[machine]") {
    const Crack cracks[] = {{Layer::Front, Mm(3, 0)}, {Layer::Front, Mm(7, 0)}};
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
    const Crack crack{Layer::Front, Mm(0.3, 0)};
    CHECK(IslandCount(CrackBoard(Joined(), {&crack, 1})) == 1);
}

TEST_CASE("crack: FormatMillimetres", "[machine]") {
    CHECK(FormatMillimetres(52'300'000) == "52.30");
    CHECK(FormatMillimetres(-1'250'000) == "-1.25");
    CHECK(FormatPoint(Layer::Back, Mm(1, 2)) == "B.Cu 1.00 2.00");
}

namespace {

Trace Line(Layer layer, Point start, Point end) {
    return {.layer = layer, .start = start, .end = end, .mid = std::nullopt, .width = 250'000};
}

// Two round pads at `a` and `b`, no Traces.
Board Pads(Point a, Point b) {
    Board board;
    board.components.push_back({.reference = "U1", .pads = {RoundPad(a)}});
    board.components.push_back({.reference = "U2", .pads = {RoundPad(b)}});
    return board;
}

// Pads at `a` and `b` joined through `corner` on the front.
Board Path2(Point a, Point corner, Point b) {
    Board board = Pads(a, b);
    board.traces = {Line(Layer::Front, a, corner), Line(Layer::Front, corner, b)};
    return board;
}

double DistanceMm(Point p, Point q) {
    return std::hypot(static_cast<double>(p.x - q.x), static_cast<double>(p.y - q.y)) / kMm;
}

Crack Place(const Board& board, Point a, Point b, double percent) {
    const auto crack = PlaceCrack(board, a, b, percent);
    REQUIRE(crack.has_value());
    return *crack;
}

}  // namespace

TEST_CASE("crack: PlaceCrack on one Trace", "[machine]") {
    const Board board = Joined();
    const Point a = Mm(0, 0);
    const Point b = Mm(10, 0);
    // First clear piece: pad radius + half width + the crack's half gap.
    const Crack first = Place(board, a, b, 0);
    CHECK(first.layer == Layer::Front);
    CHECK(first.at.y == 0);
    CHECK(first.at.x > Mm(1.1, 0).x);
    CHECK(first.at.x <= Mm(1.12, 0).x);
    const Crack last = Place(board, a, b, 100);
    CHECK(std::abs(last.at.x - (10 * kMm - first.at.x)) <= 1);
    CHECK(DistanceMm(Place(board, a, b, 50).at, Mm(5, 0)) < 0.001);
    CHECK(DistanceMm(Place(board, b, a, 30).at, Place(board, a, b, 70).at) < 0.001);
    CHECK(IslandCount(CrackBoard(board, std::span(&first, 1))) == 2);
}

TEST_CASE("crack: PlaceCrack keeps off a corner", "[machine]") {
    const Board board = Path2(Mm(0, 0), Mm(10, 0), Mm(10, 10));
    for (int percent = 0; percent <= 100; ++percent) {
        CHECK(DistanceMm(Place(board, Mm(0, 0), Mm(10, 10), percent).at, Mm(10, 0)) > 0.4);
    }
    CHECK(Place(board, Mm(0, 0), Mm(10, 10), 49).at.y == 0);
    CHECK(Place(board, Mm(0, 0), Mm(10, 10), 51).at.x == 10 * kMm);
}

TEST_CASE("crack: PlaceCrack through a via", "[machine]") {
    Board board = Pads(Mm(0, 0), Mm(10, 0));
    board.traces = {Line(Layer::Front, Mm(0, 0), Mm(5, 0)), Line(Layer::Back, Mm(5, 0), Mm(10, 0))};
    board.vias.push_back({.position = Mm(5, 0), .diameter = 600'000, .drill = 300'000});
    CHECK(Place(board, Mm(0, 0), Mm(10, 0), 25).layer == Layer::Front);
    CHECK(Place(board, Mm(0, 0), Mm(10, 0), 75).layer == Layer::Back);
    CHECK(Place(board, Mm(10, 0), Mm(0, 0), 25).layer == Layer::Back);
}

TEST_CASE("crack: PlaceCrack keeps off a T-junction", "[machine]") {
    Board board = Joined();
    board.traces.push_back(Line(Layer::Front, Mm(5, 0), Mm(5, 3)));
    for (int percent = 0; percent <= 100; ++percent) {
        CHECK(DistanceMm(Place(board, Mm(0, 0), Mm(10, 0), percent).at, Mm(5, 0)) > 0.4);
    }
}

TEST_CASE("crack: PlaceCrack skips a short stub at a pad", "[machine]") {
    const Board board = Path2(Mm(0, 0), Mm(1, 0), Mm(10, 0));
    const Crack crack = Place(board, Mm(0, 0), Mm(10, 0), 0);
    CHECK(FindCrackTrace(board, crack.layer, crack.at) == 1u);
    CHECK(crack.at.x > Mm(1.4, 0).x);
}

TEST_CASE("crack: PlaceCrack skips arcs", "[machine]") {
    Board board = Path2(Mm(0, 0), Mm(5, 0), Mm(10, 0));
    board.traces[0].mid = Mm(2.5, 1);
    CHECK(Place(board, Mm(0, 0), Mm(10, 0), 0).at.x > Mm(5, 0).x);
    board.traces[1].mid = Mm(7.5, 1);
    CHECK(PlaceCrack(board, Mm(0, 0), Mm(10, 0), 50).error() ==
          "no crackable Trace between the pads");
}

TEST_CASE("crack: PlaceCrack errors", "[machine]") {
    Board loop = Joined();
    loop.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(0, 3)));
    loop.traces.push_back(Line(Layer::Front, Mm(0, 3), Mm(10, 3)));
    loop.traces.push_back(Line(Layer::Front, Mm(10, 3), Mm(10, 0)));
    CHECK(PlaceCrack(loop, Mm(0, 0), Mm(10, 0), 50).error() == "does not separate the pads");
    CHECK(PlaceCrack(Pads(Mm(0, 0), Mm(10, 0)), Mm(0, 0), Mm(10, 0), 50).error() ==
          "pads not joined by Copper");
}
