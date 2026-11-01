#include "board/kicad_board.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

constexpr std::int64_t kMm = 1'000'000;

std::string BoardText(std::string_view body, std::string_view version = "20241229") {
    return "(kicad_pcb (version " + std::string(version) +
           ") (generator \"pcbnew\")\n"
           "  (layers (0 \"F.Cu\" signal) (2 \"B.Cu\" signal) (25 \"Edge.Cuts\" user))\n"
           "  (gr_rect (start 0 0) (end 100 80) (layer \"Edge.Cuts\"))\n" +
           std::string(body) + ")\n";
}

Board Read(std::string_view body) {
    auto board = ReadKicadBoard(BoardText(body), "t.kicad_pcb");
    if (!board) {
        for (const Diagnostic& d : board.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(board.has_value());
    return std::move(*board);
}

std::vector<std::string> Errors(std::string_view text) {
    const auto board = ReadKicadBoard(text, "t.kicad_pcb");
    REQUIRE_FALSE(board.has_value());
    std::vector<std::string> errors;
    for (const Diagnostic& d : board.error()) {
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

std::string Footprint(std::string_view at, std::string_view pads, std::string_view layer = "F.Cu") {
    return "  (footprint \"Package_DIP:DIP-14_W7.62mm\" (layer \"" + std::string(layer) +
           "\") (at " + std::string(at) +
           ")\n"
           "    (property \"Reference\" \"U1\" (at 0 0) (layer \"F.SilkS\"))\n"
           "    (fp_line (start 0 0) (end 1 1) (layer \"F.SilkS\"))\n" +
           std::string(pads) + ")\n";
}

constexpr std::string_view kTwoPads =
    "    (pad \"1\" thru_hole rect (at 0 0) (size 1.6 1.6) (drill 0.8) (layers \"*.Cu\"))\n"
    "    (pad \"2\" thru_hole oval (at 2.54 1) (size 1.6 1.2) (drill 0.8) (layers \"*.Cu\"))\n";

}  // namespace

TEST_CASE("kicad board: millimetres", "[board]") {
    CHECK(ParseMillimetres("12.7") == 12'700'000);
    CHECK(ParseMillimetres("3") == 3 * kMm);
    CHECK(ParseMillimetres("-0.000001") == -1);
    CHECK(ParseMillimetres("+.5") == kMm / 2);
    CHECK_FALSE(ParseMillimetres("1.0000001"));
    CHECK_FALSE(ParseMillimetres("abc"));
    CHECK_FALSE(ParseMillimetres(""));
    CHECK_FALSE(ParseMillimetres("-"));
}

TEST_CASE("kicad board: footprint and pad positions", "[board]") {
    const Board board = Read(Footprint("100 50", kTwoPads));
    REQUIRE(board.components.size() == 1);
    const BoardComponent& u1 = board.components[0];
    CHECK(u1.reference == "U1");
    CHECK(u1.footprint == "Package_DIP:DIP-14_W7.62mm");
    CHECK(u1.side == Layer::Front);
    CHECK(u1.line == 4);
    REQUIRE(u1.pads.size() == 2);
    CHECK(u1.pads[0].number == "1");
    CHECK(u1.pads[0].shape == PadShape::Rectangle);
    CHECK(u1.pads[0].position == Point{100 * kMm, 50 * kMm});
    CHECK(u1.pads[0].drill == 800'000);
    CHECK(u1.pads[1].shape == PadShape::Oval);
    CHECK(u1.pads[1].position == Point{102'540'000, 51 * kMm});
    CHECK(u1.pads[1].size == Point{1'600'000, 1'200'000});
}

TEST_CASE("kicad board: rotated footprints", "[board]") {
    // KiCad: y down, positive angle counter-clockwise on screen.
    const Board r90 = Read(Footprint("100 50 90", kTwoPads));
    CHECK(r90.components[0].angle == 90);
    CHECK(r90.components[0].pads[1].position == Point{101 * kMm, 47'460'000});
    const Board r180 = Read(Footprint("100 50 180", kTwoPads));
    CHECK(r180.components[0].pads[1].position == Point{97'460'000, 49 * kMm});
    const Board back = Read(Footprint("100 50", kTwoPads, "B.Cu"));
    CHECK(back.components[0].side == Layer::Back);
}

TEST_CASE("kicad board: pad shapes", "[board]") {
    const Board board = Read(Footprint(
        "0 0", "    (pad \"1\" thru_hole circle (at 0 0 45) (size 1.6 1.6) (drill 0.8))\n"
               "    (pad \"2\" thru_hole roundrect (at 0 0) (size 2 1) (drill oval 0.6 0.9) "
               "(roundrect_rratio 0.25))\n"
               "    (pad \"\" np_thru_hole circle (at 5 5) (size 3 3) (drill 3))\n"));
    const auto& pads = board.components[0].pads;
    REQUIRE(pads.size() == 2);
    CHECK(pads[0].shape == PadShape::Circle);
    CHECK(pads[0].angle == 45);
    CHECK(pads[1].shape == PadShape::RoundedRectangle);
    CHECK(pads[1].cornerRadius == 250'000);
    CHECK(pads[1].drill == 600'000);
}

TEST_CASE("kicad board: tracks, arcs, vias and outline", "[board]") {
    const Board board =
        Read("  (segment (start 1 2) (end 3 4) (width 0.25) (layer \"B.Cu\") (net 1))\n"
             "  (arc (start 0 0) (mid 1 1) (end 2 0) (width 0.3) (layer \"F.Cu\") (net 1))\n"
             "  (via (at 5 6) (size 0.6) (drill 0.3) (layers \"F.Cu\" \"B.Cu\") (net 1))\n"
             "  (gr_line (start 0 0) (end 1 0) (layer \"F.SilkS\"))\n");
    REQUIRE(board.traces.size() == 2);
    CHECK(board.traces[0].layer == Layer::Back);
    CHECK(board.traces[0].start == Point{1 * kMm, 2 * kMm});
    CHECK(board.traces[0].end == Point{3 * kMm, 4 * kMm});
    CHECK(board.traces[0].width == 250'000);
    CHECK_FALSE(board.traces[0].mid.has_value());
    CHECK(board.traces[1].mid == Point{1 * kMm, 1 * kMm});
    REQUIRE(board.vias.size() == 1);
    CHECK(board.vias[0].position == Point{5 * kMm, 6 * kMm});
    CHECK(board.vias[0].diameter == 600'000);
    CHECK(board.vias[0].drill == 300'000);
    REQUIRE(board.outline.size() == 4);
    CHECK(board.outline[1].start == Point{100 * kMm, 0});
    CHECK(board.outline[1].end == Point{100 * kMm, 80 * kMm});
}

TEST_CASE("kicad board: outline from lines", "[board]") {
    const auto board = ReadKicadBoard("(kicad_pcb (version 20250101)\n"
                                      "  (gr_line (start 0 0) (end 10 0) (layer \"Edge.Cuts\"))\n"
                                      "  (gr_arc (start 10 0) (mid 12 5) (end 10 10) "
                                      "(layer \"Edge.Cuts\")))",
                                      "t");
    REQUIRE(board.has_value());
    REQUIRE(board->outline.size() == 2);
    CHECK(board->outline[1].mid == Point{12 * kMm, 5 * kMm});
}

TEST_CASE("kicad board: errors", "[board]") {
    CHECK(Errors(BoardText("  (zone (net 1) (layer \"F.Cu\"))\n")) ==
          std::vector<std::string>{"4: copper zones are not supported"});
    CHECK(Errors("(kicad_pcb (version 20241229)\n(layers (0 \"F.Cu\" signal) (4 \"In1.Cu\" "
                 "signal))\n(gr_rect (start 0 0) (end 1 1) (layer \"Edge.Cuts\")))") ==
          std::vector<std::string>{"2: copper layer In1.Cu not supported: only F.Cu and B.Cu"});
    CHECK(Errors(BoardText(Footprint(
              "0 0", "    (pad \"1\" smd rect (at 0 0) (size 1 1) (layers \"F.Cu\"))\n"))) ==
          std::vector<std::string>{"7: U1 pad 1: only through-hole pads are supported"});
    CHECK(Errors(BoardText("  (gr_line (start 0 0) (end 1 0) (layer \"F.Cu\"))\n")) ==
          std::vector<std::string>{"4: copper graphics are not supported"});
    CHECK(Errors(BoardText("", "20240108")) ==
          std::vector<std::string>{
              "1: board file older than KiCad 9: open and save it in KiCad 9 or later"});
    CHECK(Errors("(kicad_pcb (version 20241229))") ==
          std::vector<std::string>{"1: no board outline on Edge.Cuts"});
    CHECK(Errors(BoardText("  (segment (start 1 2) (end x 4) (width 0.25) (layer \"F.Cu\"))\n")) ==
          std::vector<std::string>{"4: bad number in (end)"});
    CHECK(Errors("(kicad_pcb (version 20241229)") ==
          std::vector<std::string>{"1: unbalanced parentheses"});
}

TEST_CASE("kicad board: courtyard rect is placed like pads", "[board]") {
    const Board board =
        Read(Footprint("10 20 90", "    (fp_rect (start -1 -2) (end 3 4) (layer \"F.CrtYd\"))\n" +
                                       std::string(kTwoPads)));
    CHECK(board.components[0].courtyard == std::vector<Point>{{8 * kMm, 21 * kMm},
                                                              {8 * kMm, 17 * kMm},
                                                              {14 * kMm, 17 * kMm},
                                                              {14 * kMm, 21 * kMm}});
}

TEST_CASE("kicad board: courtyard poly", "[board]") {
    const Board board = Read(
        Footprint("10 20", "    (fp_poly (pts (xy 0 0) (xy 2 0) (xy 1 3)) (layer \"B.CrtYd\"))\n" +
                               std::string(kTwoPads)));
    CHECK(board.components[0].courtyard ==
          std::vector<Point>{{10 * kMm, 20 * kMm}, {12 * kMm, 20 * kMm}, {11 * kMm, 23 * kMm}});
}

TEST_CASE("kicad board: courtyard from lines is their placed bounding box", "[board]") {
    const std::string lines = "    (fp_line (start -1 -2) (end 3 -2) (layer \"F.CrtYd\"))\n"
                              "    (fp_line (start 3 -2) (end 3 4) (layer \"F.CrtYd\"))\n"
                              "    (fp_line (start 3 4) (end -1 4) (layer \"F.CrtYd\"))\n"
                              "    (fp_line (start 0 0) (end 9 9) (layer \"F.SilkS\"))\n";
    const Board board = Read(Footprint("10 20 90", lines + std::string(kTwoPads)));
    CHECK(board.components[0].courtyard == std::vector<Point>{{8 * kMm, 21 * kMm},
                                                              {8 * kMm, 17 * kMm},
                                                              {14 * kMm, 17 * kMm},
                                                              {14 * kMm, 21 * kMm}});
    const Board withRect = Read(
        Footprint("10 20", lines + "    (fp_rect (start 0 0) (end 1 1) (layer \"F.CrtYd\"))\n" +
                               std::string(kTwoPads)));
    CHECK(withRect.components[0].courtyard == std::vector<Point>{{10 * kMm, 20 * kMm},
                                                                 {11 * kMm, 20 * kMm},
                                                                 {11 * kMm, 21 * kMm},
                                                                 {10 * kMm, 21 * kMm}});
}

TEST_CASE("kicad board: no courtyard", "[board]") {
    const Board board = Read(Footprint("10 20", kTwoPads));
    CHECK(board.components[0].courtyard.empty());
}
