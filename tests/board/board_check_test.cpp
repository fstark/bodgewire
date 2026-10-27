#include "board/board_check.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "board/kicad_netlist.hpp"

using namespace bw;

namespace {

using Lines = std::vector<std::string>;

constexpr std::int64_t kMm = 1'000'000;

constexpr std::string_view kNetlist = "part R1 R\n"
                                      "part R2 R\n"
                                      "part X1 OSC freq=1MHz\n"
                                      "net VCC R1.1 R2.1\n"
                                      "net A R1.2 X1.OUT\n"
                                      "net B R2.2\n";

Point Mm(std::int64_t x, std::int64_t y) { return {x * kMm, y * kMm}; }

Pad RoundPad(std::string number, Point at) {
    return {.number = std::move(number),
            .position = at,
            .angle = 0,
            .size = Mm(1, 1),
            .cornerRadius = 0,
            .drill = 0,
            .shape = PadShape::Circle};
}

void Route(Board& board, std::vector<Point> points, Layer layer = Layer::Front) {
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        board.traces.push_back({layer, points[i], points[i + 1], std::nullopt, 250'000});
    }
}

// Pads: R1 (0,0) (0,10); R2 (10,0) (10,10); X1 pin 1 (20,0), 7 (20,10), 8 (30,10), 14 (30,0).
Board GoodBoard() {
    Board board;
    const std::string resistor(KicadFootprint("axial"));
    board.components.push_back({.reference = "R1",
                                .footprint = resistor,
                                .pads = {RoundPad("1", Mm(0, 0)), RoundPad("2", Mm(0, 10))}});
    board.components.push_back({.reference = "R2",
                                .footprint = resistor,
                                .pads = {RoundPad("1", Mm(10, 0)), RoundPad("2", Mm(10, 10))}});
    board.components.push_back({.reference = "X1",
                                .footprint = std::string(KicadFootprint("DIP-14")),
                                .pads = {RoundPad("1", Mm(20, 0)), RoundPad("7", Mm(20, 10)),
                                         RoundPad("8", Mm(30, 10)), RoundPad("14", Mm(30, 0))}});
    Route(board, {Mm(0, 0), Mm(10, 0)});
    Route(board, {Mm(10, 0), Mm(10, -5), Mm(30, -5), Mm(30, 0)}, Layer::Back);
    Route(board, {Mm(0, 10), Mm(0, 15), Mm(30, 15), Mm(30, 10)});
    return board;
}

Lines Check(const Board& board) {
    const auto loaded = LoadNetlist(kNetlist, "t.bwn");
    REQUIRE(loaded.has_value());
    return CheckBoard(**loaded, board);
}

}  // namespace

TEST_CASE("board check: ok", "[board]") { CHECK(Check(GoodBoard()).empty()); }

TEST_CASE("board check: open", "[board]") {
    Board board = GoodBoard();
    board.traces.erase(board.traces.begin() + 3);
    CHECK(Check(board) == Lines{"open VCC: {R1.1, R2.1} | {X1.14}"});
}

TEST_CASE("board check: short", "[board]") {
    Board board = GoodBoard();
    Route(board, {Mm(10, 10), Mm(10, 15)});
    CHECK(Check(board) == Lines{"short: A (R1.2), B (R2.2)"});
}

TEST_CASE("board check: copper on an unconnected pad", "[board]") {
    Board board = GoodBoard();
    Route(board, {Mm(20, 0), Mm(22, 0)});
    CHECK(Check(board) == Lines{"X1.1: copper on an unconnected pad"});
}

TEST_CASE("board check: missing, extra and wrong components", "[board]") {
    Board board = GoodBoard();
    board.components.erase(board.components.begin() + 1);
    board.components.push_back({.reference = "U9", .footprint = "x"});
    board.components[1].footprint = KicadFootprint("DIP-16");
    // Without R2's through-hole pad, the front and back VCC traces no longer meet.
    CHECK(Check(board) ==
          Lines{"R2: missing from board", "U9: not in netlist",
                "X1: footprint Package_DIP:DIP-16_W7.62mm, expected Package_DIP:DIP-14_W7.62mm",
                "open VCC: {R1.1} | {X1.14}"});
}

TEST_CASE("board check: missing pad", "[board]") {
    Board board = GoodBoard();
    board.components[2].pads.erase(board.components[2].pads.begin() + 2);
    CHECK(Check(board) == Lines{"X1.8: no pad 8 on footprint Package_DIP:DIP-14_W7.62mm"});
}

TEST_CASE("board check: on the board twice", "[board]") {
    Board board = GoodBoard();
    board.components.push_back(board.components[0]);
    board.components.back().pads.clear();
    CHECK(Check(board).front() == "R1: on the board twice");
}

TEST_CASE("board check: a board needs one POWER connector", "[board]") {
    const auto problems = [](std::string_view netlist) {
        const auto loaded = LoadNetlist(netlist, "t.bwn");
        REQUIRE(loaded.has_value());
        return BoardNetlistProblems(**loaded);
    };
    const Lines missing{"a board needs one POWER connector"};
    CHECK(problems("part U1 74LS00\n") == missing);
    CHECK(problems("part J1 POWER\n").empty());
    CHECK(problems("part J1 POWER\npart J2 POWER\n") == missing);
}
