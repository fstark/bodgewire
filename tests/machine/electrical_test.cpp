#include "machine/electrical.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "test_points.hpp"

using namespace bw;

namespace {

struct Design {
    Netlist netlist;
    std::unique_ptr<LoadedCircuit> circuit;
    Board board;
};

Design LoadDesign(const std::string& name) {
    std::ifstream in(std::filesystem::path(BW_NETLIST_DIR) / (name + ".bwn"));
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto netlist = ParseNetlist(text, name + ".bwn");
    REQUIRE(netlist.has_value());
    auto circuit = BuildCircuit(*netlist);
    REQUIRE(circuit.has_value());
    auto board =
        ReadKicadBoardFile(std::filesystem::path(BW_BOARD_DIR) / name / (name + ".kicad_pcb"));
    REQUIRE(board.has_value());
    return {std::move(*netlist), std::move(*circuit), std::move(*board)};
}

// Net name → its Pins as "REF.N", from a built circuit's connections.
std::map<std::string, std::set<std::string>> NetPins(const LoadedCircuit& loaded) {
    std::map<std::string, std::set<std::string>> nets;
    for (const Connection& c : loaded.connections) {
        nets[c.net].insert(c.reference + "." + std::to_string(c.pin + 1));
    }
    return nets;
}

std::vector<std::string> NetNamesById(const LoadedCircuit& loaded) {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < loaded.circuit.netCount(); ++i) {
        names.emplace_back(loaded.circuit.netName(NetId{static_cast<std::uint32_t>(i)}));
    }
    return names;
}

std::string NetOf(const Netlist& netlist, const std::string& pin) {
    for (const NetDecl& net : netlist.nets) {
        for (const PinRef& ref : net.pins) {
            if (ref.reference + "." + ref.pin == pin) {
                return net.name;
            }
        }
    }
    return "";
}

}  // namespace

TEST_CASE("electrical: uncracked board gives the design Nets", "[machine]") {
    for (const std::string name : {"test_pattern", "counter"}) {
        INFO(name);
        const Design design = LoadDesign(name);
        const auto electrical =
            BuildCircuit(ElectricalNetlist(design.netlist, *design.circuit, design.board));
        REQUIRE(electrical.has_value());
        CHECK(NetPins(**electrical) == NetPins(*design.circuit));
        CHECK(NetNamesById(**electrical) == NetNamesById(*design.circuit));
    }
}

TEST_CASE("electrical: crack on H0 splits it", "[machine]") {
    const Design design = LoadDesign("test_pattern");
    const CrackDecl crack{1, Layer::Front, kH0Crack};
    const Netlist netlist =
        ElectricalNetlist(design.netlist, *design.circuit, CrackBoard(design.board, {&crack, 1}));
    CHECK(NetOf(netlist, "U1.14") == "H0");    // QA
    CHECK(NetOf(netlist, "U13.6") == "H0~2");  // 2B
}

TEST_CASE("electrical: island names match the electrical netlist", "[machine]") {
    const Design design = LoadDesign("test_pattern");
    const CrackDecl crack{1, Layer::Front, kH0Crack};
    const Board board = CrackBoard(design.board, {&crack, 1});
    const Islands islands = FindIslands(board);
    const auto names = IslandNetNames(*design.circuit, board, islands);
    REQUIRE(names.size() == islands.islandSize.size());
    std::map<std::string, std::uint32_t> padIsland;
    for (std::size_t k = 0; k < board.components.size(); ++k) {
        for (std::size_t p = 0; p < board.components[k].pads.size(); ++p) {
            padIsland[board.components[k].reference + "." + board.components[k].pads[p].number] =
                islands.padIsland[k][p];
        }
    }
    std::size_t checked = 0;
    for (const NetDecl& net : ElectricalNetlist(design.netlist, *design.circuit, board).nets) {
        for (const PinRef& ref : net.pins) {
            INFO(ref.reference << "." << ref.pin);
            CHECK(names[padIsland.at(ref.reference + "." + ref.pin)] == net.name);
            ++checked;
        }
    }
    CHECK(checked > 100);
}

TEST_CASE("electrical: pull-up cut from VCC is left out", "[machine]") {
    const Design design = LoadDesign("counter");
    const Netlist netlist =
        ElectricalNetlist(design.netlist, *design.circuit, CrackBoard(design.board, kR1VccCracks));
    for (const PartDecl& part : netlist.parts) {
        CHECK(part.reference != "R1");
    }
    CHECK(NetOf(netlist, "R1.1").empty());
    CHECK(BuildCircuit(netlist).has_value());
}
