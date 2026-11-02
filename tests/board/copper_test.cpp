#include "board/copper.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

constexpr std::int64_t kMm = 1'000'000;

Point Mm(double x, double y) {
    return {static_cast<std::int64_t>(x * kMm), static_cast<std::int64_t>(y * kMm)};
}

Pad RoundPad(std::string number, Point at) {
    return {.number = std::move(number),
            .position = at,
            .angle = 0,
            .size = {1'600'000, 1'600'000},
            .cornerRadius = 0,
            .drill = 800'000,
            .shape = PadShape::Circle};
}

// Two components with one pad each, 10 mm apart on y = 0.
Board TwoPads() {
    Board board;
    board.components.push_back({.reference = "U1", .pads = {RoundPad("1", Mm(0, 0))}});
    board.components.push_back({.reference = "U2", .pads = {RoundPad("1", Mm(10, 0))}});
    return board;
}

Trace Line(Layer layer, Point start, Point end) {
    return {.layer = layer, .start = start, .end = end, .mid = std::nullopt, .width = 250'000};
}

bool Joined(const Board& board) {
    const Islands islands = FindIslands(board);
    return islands.padIsland[0][0] == islands.padIsland[1][0];
}

}  // namespace

TEST_CASE("copper: separate pads", "[board]") {
    const Islands islands = FindIslands(TwoPads());
    CHECK(islands.padIsland[0][0] != islands.padIsland[1][0]);
    CHECK(islands.islandSize == std::vector<std::uint32_t>{1, 1});
}

TEST_CASE("copper: trace between pad centres joins them", "[board]") {
    for (const Layer layer : {Layer::Front, Layer::Back}) {
        Board board = TwoPads();
        board.traces.push_back(Line(layer, Mm(0, 0), Mm(10, 0)));
        CHECK(Joined(board));
        CHECK(FindIslands(board).islandSize == std::vector<std::uint32_t>{3});
    }
}

TEST_CASE("copper: bodge wire joins two pads", "[board]") {
    Board board = TwoPads();
    board.bodgeWires = {{Mm(0, 0), Mm(10, 0)}};
    CHECK(Joined(board));
    CHECK(FindIslands(board).islandSize == std::vector<std::uint32_t>{2});
}

TEST_CASE("copper: via joins layers", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(5, 5)));
    board.traces.push_back(Line(Layer::Back, Mm(5, 5), Mm(10, 0)));
    CHECK_FALSE(Joined(board));
    board.vias.push_back({.position = Mm(5, 5), .diameter = 600'000, .drill = 300'000});
    CHECK(Joined(board));
}

TEST_CASE("copper: traces on different layers do not join where they cross", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(5, 0)));
    board.traces.push_back(Line(Layer::Back, Mm(5, -1), Mm(5, 1)));
    board.traces.push_back(Line(Layer::Back, Mm(5, 1), Mm(10, 0)));
    CHECK_FALSE(Joined(board));
}

TEST_CASE("copper: T-junction", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(0, 5)));
    board.traces.push_back(Line(Layer::Front, Mm(-5, 5), Mm(15, 5)));
    board.traces.push_back(Line(Layer::Front, Mm(10, 5), Mm(10, 0)));
    CHECK(Joined(board));
}

TEST_CASE("copper: arc trace", "[board]") {
    Board board = TwoPads();
    board.traces.push_back({.layer = Layer::Front,
                            .start = Mm(0, 0),
                            .end = Mm(10, 0),
                            .mid = Mm(5, -5),
                            .width = 250'000});
    CHECK(Joined(board));
    // A pad on the arc's far side, not on the chord.
    board.components.push_back({.reference = "U3", .pads = {RoundPad("1", Mm(5, -5))}});
    board.components.push_back({.reference = "U4", .pads = {RoundPad("1", Mm(5, 5))}});
    const Islands islands = FindIslands(board);
    CHECK(islands.padIsland[2][0] == islands.padIsland[0][0]);
    CHECK(islands.padIsland[3][0] != islands.padIsland[0][0]);
}

TEST_CASE("copper: near miss and touching edge", "[board]") {
    // Pad radius 0.8 mm, trace half-width 0.125 mm: touching at 0.925 mm from the centre.
    Board miss = TwoPads();
    miss.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(9.0, 0)));
    CHECK_FALSE(Joined(miss));
    Board touch = TwoPads();
    touch.traces.push_back(Line(Layer::Front, Mm(0, 0), Point{9'075'000, 0}));
    CHECK(Joined(touch));
}

TEST_CASE("copper: rotated rectangle pad", "[board]") {
    // 4 x 1 mm pad at (10, 0): a trace ending at (10, 1.8) reaches it only when rotated 90°.
    for (const double angle : {0.0, 90.0}) {
        Board board = TwoPads();
        Pad& pad = board.components[1].pads[0];
        pad.shape = PadShape::Rectangle;
        pad.size = Mm(4, 1);
        pad.angle = angle;
        board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(0, 1.8)));
        board.traces.push_back(Line(Layer::Front, Mm(0, 1.8), Mm(10, 1.8)));
        CHECK(Joined(board) == (angle == 90.0));
    }
}

TEST_CASE("copper: oval pad along its long axis", "[board]") {
    Board board = TwoPads();
    Pad& pad = board.components[1].pads[0];
    pad.shape = PadShape::Oval;
    pad.size = Mm(1, 4);
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(0, 1.8)));
    board.traces.push_back(Line(Layer::Front, Mm(0, 1.8), Mm(10, 1.8)));
    CHECK(Joined(board));
}

TEST_CASE("copper: Covers trace caps and width", "[board]") {
    const Trace trace = Line(Layer::Front, Mm(0, 0), Mm(10, 0));  // half width 0.125 mm
    CHECK(Covers(trace, Mm(5, 0.12)));
    CHECK_FALSE(Covers(trace, Mm(5, 0.13)));
    CHECK(Covers(trace, Mm(-0.12, 0)));
    CHECK_FALSE(Covers(trace, Mm(-0.13, 0)));
    CHECK_FALSE(Covers(trace, Mm(-0.1, 0.1)));
}

TEST_CASE("copper: Covers rotated rectangular pad", "[board]") {
    Pad pad = RoundPad("1", Mm(0, 0));
    pad.shape = PadShape::Rectangle;
    pad.size = Mm(2, 1);
    pad.angle = 90;  // now 1 mm wide, 2 mm tall
    CHECK(Covers(pad, Mm(0.45, 0.95)));
    CHECK_FALSE(Covers(pad, Mm(0.55, 0)));
    CHECK_FALSE(Covers(pad, Mm(0, 1.05)));
}

TEST_CASE("copper: Covers via", "[board]") {
    const Via via{.position = Mm(5, 5), .diameter = 600'000, .drill = 300'000};
    CHECK(Covers(via, Mm(5.29, 5)));
    CHECK(Covers(via, Mm(5, 5)));
    CHECK_FALSE(Covers(via, Mm(5.31, 5)));
}

TEST_CASE("copper: via and trace islands", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(5, 5)));
    board.vias.push_back({.position = Mm(5, 5), .diameter = 600'000, .drill = 300'000});
    const Islands islands = FindIslands(board);
    REQUIRE(islands.viaIsland.size() == 1);
    REQUIRE(islands.traceIsland.size() == 1);
    CHECK(islands.viaIsland[0] == islands.padIsland[0][0]);
    CHECK(islands.traceIsland[0] == islands.padIsland[0][0]);
    CHECK(islands.padIsland[1][0] != islands.padIsland[0][0]);
}

TEST_CASE("copper: arc polyline ends", "[board]") {
    const auto points = ArcPolyline(Mm(0, 0), Mm(5, 5), Mm(10, 0));
    REQUIRE(points.size() > 2);
    CHECK(points.front() == Mm(0, 0));
    CHECK(points.back() == Mm(10, 0));
    bool nearMid = false;
    for (const Point p : points) {
        nearMid =
            nearMid || (std::abs(p.x - 5 * kMm) < 100'000 && std::abs(p.y - 5 * kMm) < 100'000);
    }
    CHECK(nearMid);
}

TEST_CASE("copper: graph of two pads and a Trace", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(10, 0)));
    const CopperGraph graph = BuildCopperGraph(board);
    CHECK(graph.pads == 2);
    CHECK(graph.vias == 0);
    REQUIRE(graph.touching.size() == 3);
    CHECK(graph.touching[0] == std::vector<std::uint32_t>{2});
    CHECK(graph.touching[1] == std::vector<std::uint32_t>{2});
    CHECK(graph.touching[2] == std::vector<std::uint32_t>{0, 1});
}

TEST_CASE("copper: a Bodge Wire joins its pads in the graph", "[board]") {
    Board board = TwoPads();
    board.bodgeWires = {{Mm(0, 0), Mm(10, 0)}};
    const CopperGraph graph = BuildCopperGraph(board);
    CHECK(graph.touching[0] == std::vector<std::uint32_t>{1});
    CHECK(graph.touching[1] == std::vector<std::uint32_t>{0});
    CHECK(PadItemAt(board, Mm(10, 0)) == 1u);
    CHECK_FALSE(PadItemAt(board, Mm(5, 0)));
}

TEST_CASE("copper: Touches", "[board]") {
    Board board = TwoPads();
    board.traces.push_back(Line(Layer::Front, Mm(0, 0), Mm(10, 0)));
    const Trace middle = Line(Layer::Front, Mm(4, 0), Mm(6, 0));
    CHECK(Touches(board, 2, middle));
    CHECK_FALSE(Touches(board, 0, middle));
    CHECK_FALSE(Touches(board, 1, middle));
    CHECK(Touches(board, 0, Line(Layer::Front, Mm(0.5, 0), Mm(1.5, 0))));
    CHECK_FALSE(Touches(board, 2, Line(Layer::Back, Mm(4, 0), Mm(6, 0))));
}
