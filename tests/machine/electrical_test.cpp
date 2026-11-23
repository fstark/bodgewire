#include "machine/electrical.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"
#include "test_points.hpp"

using namespace bw;

namespace {

struct Design {
    Netlist netlist;
    std::unique_ptr<LoadedCircuit> circuit;
    Board board;
};

Design LoadDesign(const std::string& name) {
    std::ifstream in(test::NetlistPath(name));
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto netlist = ParseNetlist(text, name + ".bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, test::MachineDir(name));
    auto circuit = BuildCircuit(*netlist);
    REQUIRE(circuit.has_value());
    auto board = ReadKicadBoardFile(test::BoardPath(name));
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
    for (const std::string name :
         {"test_pattern", "counter", "chaser", "dice", "stopwatch", "ball", "knob", "tennis",
          "message", "typewriter", "led_computer", "bw1", "bw2001", "bw80"}) {
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
    const Crack crack = H0Crack();
    const Netlist netlist =
        ElectricalNetlist(design.netlist, *design.circuit, CrackBoard(design.board, {&crack, 1}));
    CHECK(NetOf(netlist, "U1.14") == "H0");    // QA
    CHECK(NetOf(netlist, "U13.6") == "H0~2");  // 2B
}

TEST_CASE("electrical: a dry pin is on no Net", "[machine]") {
    const Design design = LoadDesign("test_pattern");
    const PinRef dry[] = {{"U13", "6"}};  // 2B on H0
    const auto electrical =
        BuildCircuit(ElectricalNetlist(design.netlist, *design.circuit, design.board, dry));
    REQUIRE(electrical.has_value());
    auto expected = NetPins(*design.circuit);
    expected["H0"].erase("U13.6");
    CHECK(NetPins(**electrical) == expected);
    CHECK(PinNet(**electrical, "U13", "2B") != (*electrical)->nets.at("H0"));
}

TEST_CASE("electrical: island names match the electrical netlist", "[machine]") {
    const Design design = LoadDesign("test_pattern");
    const Crack crack = H0Crack();
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
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit,
                                              CrackBoard(design.board, {&R1VccCrack(), 1}));
    for (const PartDecl& part : netlist.parts) {
        CHECK(part.reference != "R1");
    }
    CHECK(NetOf(netlist, "R1.1").empty());
    CHECK(BuildCircuit(netlist).has_value());
}

TEST_CASE("electrical: button cut from GND joins its signal to a dead island", "[machine]") {
    const Design design = LoadDesign("dice");
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit,
                                              CrackBoard(design.board, {&Sw1GndCrack(), 1}));
    CHECK(
        std::ranges::any_of(netlist.parts, [](const PartDecl& p) { return p.reference == "SW1"; }));
    CHECK(NetOf(netlist, "SW1.1") == "BTN_N");
    CHECK(NetOf(netlist, "SW1.2").starts_with("GND~"));
    const auto built = BuildCircuit(netlist);
    REQUIRE(built.has_value());
    Switch* button = SwitchOf(**built, "SW1");
    REQUIRE(button != nullptr);
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    button->setPressed(true);
    sim.wake(button->id());
    sim.runUntil(1'000'000);
    CHECK(sim.value((*built)->nets.at("BTN_N")) == LogicValue::One);  // pulled up, still
}

TEST_CASE("electrical: button alone on its signal island stays in", "[machine]") {
    const Design design = LoadDesign("dice");
    const Crack crack = BtnNCrack();
    const Netlist netlist =
        ElectricalNetlist(design.netlist, *design.circuit, CrackBoard(design.board, {&crack, 1}));
    CHECK(NetOf(netlist, "SW1.1") == "BTN_N");
    CHECK(NetOf(netlist, "U4.3") == "BTN_N~2");
    CHECK(NetOf(netlist, "SW1.2") == "GND");
    const auto built = BuildCircuit(netlist);
    REQUIRE(built.has_value());
    CHECK(SwitchOf(**built, "SW1") != nullptr);
}

namespace {

Machine PowerMachine(std::vector<CrackDecl> cracks, std::vector<BridgeDecl> bridges) {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    puzzle.cracks = std::move(cracks);
    puzzle.bridges = std::move(bridges);
    auto machine = LoadMachine(puzzle, test::MachineDir(puzzle.machine));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

const std::vector<std::string> kAllChips{"X1", "U1", "U2",  "U3",  "U4",  "U5",  "U6", "U7",
                                         "U8", "U9", "U10", "U11", "U12", "U13", "U14"};

}  // namespace

TEST_CASE("power: fault-free board powers every chip", "[machine]") {
    const Machine machine = PowerMachine({}, {});
    CHECK(machine.power().unpowered.empty());
    CHECK_FALSE(machine.power().deadRail);
}

TEST_CASE("power: cracked VCC stub leaves one chip Unpowered", "[machine]") {
    const Machine machine = PowerMachine({{2, {"U6", "VCC"}, {"U10", "VCC"}, 24}}, {});
    CHECK(machine.power().unpowered == std::vector<std::string>{"U6"});
    CHECK_FALSE(machine.power().deadRail);
}

TEST_CASE("power: VCC bridged to GND is a Dead Rail", "[machine]") {
    Machine machine = PowerMachine({}, {{2, {"U1", "7"}, {"U1", "8"}}});
    CHECK(machine.power().deadRail);
    CHECK(machine.power().unpowered == kAllChips);
    REQUIRE(machine.removeBridge({"U1", "7"}, {"U1", "8"}));
    CHECK_FALSE(machine.power().deadRail);
    CHECK(machine.power().unpowered.empty());
}

TEST_CASE("power: GND pin wired to VCC", "[machine]") {
    Machine machine = PowerMachine({}, {});
    REQUIRE(machine.addWire({"U9", "7"}, {"U9", "14"}));
    CHECK(machine.power().deadRail);
    CHECK(std::ranges::find(machine.power().unpowered, "U9") != machine.power().unpowered.end());
}

namespace {

constexpr std::int64_t kMm = 1'000'000;

Point Mm(double x, double y) {
    return {static_cast<std::int64_t>(x * kMm), static_cast<std::int64_t>(y * kMm)};
}

using Rows = std::vector<std::vector<std::pair<std::string, std::string>>>;

// A small netlist on a Board drawn here (no KiCad layout): each Net a row of round pads at
// y = 0, 10, 20 … mm, 10 mm apart, joined by one front Trace.
Board RowBoard(const Rows& rows) {
    Board board;
    std::map<std::string, std::size_t> component;
    for (std::size_t y = 0; y < rows.size(); ++y) {
        for (std::size_t x = 0; x < rows[y].size(); ++x) {
            const auto& [reference, number] = rows[y][x];
            if (!component.contains(reference)) {
                component[reference] = board.components.size();
                board.components.push_back({.reference = reference});
            }
            board.components[component[reference]].pads.push_back(
                {.number = number,
                 .position = Mm(10.0 * static_cast<double>(x), 10.0 * static_cast<double>(y)),
                 .angle = 0,
                 .size = {1'600'000, 1'600'000},
                 .cornerRadius = 0,
                 .drill = 800'000,
                 .shape = PadShape::Circle});
        }
        if (rows[y].size() > 1) {
            board.traces.push_back({.layer = Layer::Front,
                                    .start = Mm(0, 10.0 * static_cast<double>(y)),
                                    .end = Mm(10.0 * static_cast<double>(rows[y].size() - 1),
                                              10.0 * static_cast<double>(y)),
                                    .mid = {},
                                    .width = 250'000});
        }
    }
    return board;
}

// tests/data/series_bench.bwn.
Board SeriesBoard() {
    return RowBoard({
        {{"J1", "1"}, {"U1", "1"}, {"R2", "1"}, {"U1", "14"}},                             // VCC
        {{"J1", "2"}, {"U1", "5"}, {"U1", "7"}, {"U1", "9"}, {"U1", "11"}, {"U1", "13"}},  // GND
        {{"U1", "2"}, {"R1", "1"}},                                                        // A
        {{"R1", "2"}, {"R2", "2"}, {"U1", "3"}},                                           // B
        {{"U1", "4"}},                                                                     // B_N
    });
}

// tests/data/capacitor_bench.bwn (P5v).
Board CapacitorBoard() {
    return RowBoard({
        {{"J1", "1"}, {"U1", "14"}},  // VCC
        {{"J1", "2"},
         {"R1", "2"},
         {"U1", "1"},
         {"U1", "5"},
         {"U1", "7"},
         {"U1", "9"},  // GND
         {"U1", "11"},
         {"U1", "13"}},
        {{"U1", "2"}, {"C1", "1"}},               // A
        {{"C1", "2"}, {"R1", "1"}, {"U1", "3"}},  // B
        {{"U1", "4"}},                            // B_N
    });
}

struct SeriesDesign {
    Netlist netlist;
    std::unique_ptr<LoadedCircuit> circuit;
};

SeriesDesign LoadSeries(const std::string& file = "series_bench.bwn") {
    const auto path = std::filesystem::path(BW_ROOT_DIR) / "tests/data" / file;
    std::ifstream in(path);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto netlist = ParseNetlist(text, file);
    REQUIRE(netlist.has_value());
    auto circuit = BuildCircuit(*netlist);
    REQUIRE(circuit.has_value());
    return {std::move(*netlist), std::move(*circuit)};
}

}  // namespace

TEST_CASE("electrical: a series resistor stays between its two islands", "[machine]") {
    const SeriesDesign design = LoadSeries();
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit, SeriesBoard());
    CHECK(NetOf(netlist, "R1.1") == "A");
    CHECK(NetOf(netlist, "R1.2") == "B");
    CHECK(NetOf(netlist, "R2.2") == "B");
    CHECK(BuildCircuit(netlist).has_value());
}

TEST_CASE("electrical: a series resistor bridged across is left out", "[machine]") {
    const SeriesDesign design = LoadSeries();
    Board board = SeriesBoard();
    board.padJoins.push_back({Mm(10, 20), Mm(0, 30)});  // R1.1 to R1.2: a Solder Bridge
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit, board);
    for (const PartDecl& part : netlist.parts) {
        CHECK(part.reference != "R1");
    }
    CHECK(NetOf(netlist, "R1.1").empty());
    CHECK(NetOf(netlist, "U1.2") == NetOf(netlist, "U1.3"));  // A and B are one island now
    CHECK(BuildCircuit(netlist).has_value());
}

TEST_CASE("electrical: a capacitor stays between its two islands", "[machine]") {
    const SeriesDesign design = LoadSeries("capacitor_bench.bwn");
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit, CapacitorBoard());
    CHECK(NetOf(netlist, "C1.1") == "A");
    CHECK(NetOf(netlist, "C1.2") == "B");
    CHECK(BuildCircuit(netlist).has_value());
}

TEST_CASE("electrical: a capacitor bridged across is left out", "[machine]") {
    const SeriesDesign design = LoadSeries("capacitor_bench.bwn");
    Board board = CapacitorBoard();
    board.padJoins.push_back({Mm(10, 20), Mm(0, 30)});  // C1.1 to C1.2: a Solder Bridge
    const Netlist netlist = ElectricalNetlist(design.netlist, *design.circuit, board);
    for (const PartDecl& part : netlist.parts) {
        CHECK(part.reference != "C1");
    }
    CHECK(NetOf(netlist, "C1.1").empty());
    CHECK(BuildCircuit(netlist).has_value());
}
