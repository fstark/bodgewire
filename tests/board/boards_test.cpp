#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/board_check.hpp"
#include "board/kicad_board.hpp"

using namespace bw;

namespace {

struct Loaded {
    std::unique_ptr<LoadedCircuit> circuit;
    Board board;
};

Loaded Load(const std::string& name) {
    auto circuit = LoadNetlistFile(test::NetlistPath(name));
    REQUIRE(circuit.has_value());
    auto board = ReadKicadBoardFile(test::BoardPath(name));
    if (!board) {
        for (const Diagnostic& d : board.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(board.has_value());
    return {std::move(*circuit), std::move(*board)};
}

}  // namespace

TEST_CASE("boards: counter matches its netlist", "[board]") {
    const Loaded counter = Load("counter");
    CHECK(CheckBoard(*counter.circuit, counter.board).empty());
}

TEST_CASE("boards: test pattern matches its netlist", "[board]") {
    const Loaded pattern = Load("test_pattern");
    CHECK_FALSE(pattern.board.vias.empty());
    CHECK(CheckBoard(*pattern.circuit, pattern.board).empty());
}

TEST_CASE("boards: chaser matches its netlist", "[board]") {
    const Loaded chaser = Load("chaser");
    CHECK(CheckBoard(*chaser.circuit, chaser.board).empty());
}

TEST_CASE("boards: dice matches its netlist", "[board]") {
    const Loaded dice = Load("dice");
    CHECK(CheckBoard(*dice.circuit, dice.board).empty());
}

TEST_CASE("boards: stopwatch matches its netlist", "[board]") {
    const Loaded stopwatch = Load("stopwatch");
    CHECK(CheckBoard(*stopwatch.circuit, stopwatch.board).empty());
}

TEST_CASE("boards: ball matches its netlist", "[board]") {
    const Loaded ball = Load("ball");
    CHECK(CheckBoard(*ball.circuit, ball.board).empty());
}

TEST_CASE("boards: knob matches its netlist", "[board]") {
    const Loaded knob = Load("knob");
    CHECK(CheckBoard(*knob.circuit, knob.board).empty());
}

TEST_CASE("boards: tennis matches its netlist", "[board]") {
    const Loaded tennis = Load("tennis");
    CHECK(CheckBoard(*tennis.circuit, tennis.board).empty());
}

TEST_CASE("boards: typewriter matches its netlist", "[board]") {
    const Loaded typewriter = Load("typewriter");
    CHECK(CheckBoard(*typewriter.circuit, typewriter.board).empty());
}

TEST_CASE("boards: led_computer matches its netlist", "[board]") {
    const Loaded computer = Load("led_computer");
    CHECK(CheckBoard(*computer.circuit, computer.board).empty());
}

TEST_CASE("boards: bw1 matches its netlist", "[board]") {
    const Loaded bw1 = Load("bw1");
    CHECK(CheckBoard(*bw1.circuit, bw1.board).empty());
}

TEST_CASE("boards: bw2001 matches its netlist", "[board]") {
    const Loaded bw2001 = Load("bw2001");
    CHECK(CheckBoard(*bw2001.circuit, bw2001.board).empty());
}

TEST_CASE("boards: bw80 matches its netlist", "[board]") {
    const Loaded bw80 = Load("bw80");
    CHECK(CheckBoard(*bw80.circuit, bw80.board).empty());
}

TEST_CASE("boards: the heartbeat fixture matches its netlist", "[board]") {
    auto circuit = LoadNetlistFile(test::HeartbeatNetlistPath());
    REQUIRE(circuit.has_value());
    auto board = ReadKicadBoardFile(test::HeartbeatNetlistPath().parent_path() / "board" /
                                    "heartbeat.kicad_pcb");
    REQUIRE(board.has_value());
    const Loaded heartbeat{std::move(*circuit), std::move(*board)};
    CHECK(CheckBoard(*heartbeat.circuit, heartbeat.board).empty());
}

TEST_CASE("boards: message matches its netlist", "[board]") {
    const Loaded message = Load("message");
    CHECK(CheckBoard(*message.circuit, message.board).empty());
}

TEST_CASE("boards: counter with a track removed has one open", "[board]") {
    Loaded counter = Load("counter");
    REQUIRE_FALSE(counter.board.traces.empty());
    counter.board.traces.erase(counter.board.traces.begin());
    const auto problems = CheckBoard(*counter.circuit, counter.board);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].starts_with("open "));
}

namespace {

bool InPolygon(const std::vector<Point>& polygon, Point p) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Point a = polygon[i];
        const Point b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x = static_cast<double>(b.x - a.x) * static_cast<double>(p.y - a.y) /
                                 static_cast<double>(b.y - a.y) +
                             static_cast<double>(a.x);
            inside = inside != (static_cast<double>(p.x) < x);
        }
    }
    return inside;
}

}  // namespace

TEST_CASE("boards: every Component has a courtyard around its pads", "[board]") {
    for (const std::string name :
         {"counter", "test_pattern", "chaser", "dice", "stopwatch", "ball", "knob", "tennis",
          "message", "typewriter", "led_computer", "bw1", "bw2001", "bw80"}) {
        const Loaded loaded = Load(name);
        for (const BoardComponent& component : loaded.board.components) {
            INFO(name << " " << component.reference);
            REQUIRE(component.courtyard.size() >= 3);
            for (const Pad& pad : component.pads) {
                CHECK(InPolygon(component.courtyard, pad.position));
            }
        }
    }
}
