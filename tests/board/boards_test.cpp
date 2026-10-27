#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "board/board_check.hpp"
#include "board/kicad_board.hpp"

using namespace bw;

namespace {

struct Loaded {
    std::unique_ptr<LoadedCircuit> circuit;
    Board board;
};

Loaded Load(const std::string& name) {
    auto circuit = LoadNetlistFile(std::string(BW_NETLIST_DIR) + "/" + name + ".bwn");
    REQUIRE(circuit.has_value());
    auto board =
        ReadKicadBoardFile(std::string(BW_BOARD_DIR) + "/" + name + "/" + name + ".kicad_pcb");
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

TEST_CASE("boards: counter with a track removed has one open", "[board]") {
    Loaded counter = Load("counter");
    REQUIRE_FALSE(counter.board.traces.empty());
    counter.board.traces.erase(counter.board.traces.begin());
    const auto problems = CheckBoard(*counter.circuit, counter.board);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].starts_with("open "));
}
