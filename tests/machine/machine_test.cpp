#include "machine/machine.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "board/copper.hpp"
#include "machine/crack.hpp"
#include "runner/runner.hpp"
#include "test_points.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using Lines = std::vector<std::string>;

std::filesystem::path Root() { return std::filesystem::path(BW_NETLIST_DIR).parent_path(); }

Puzzle TestPattern() {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    return puzzle;
}

Lines Errors(const Puzzle& puzzle) {
    const auto result = LoadMachine(puzzle, Root());
    REQUIRE_FALSE(result.has_value());
    Lines errors;
    for (const Diagnostic& d : result.error()) {
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

std::vector<std::string> NetNames(const LoadedCircuit& loaded) {
    std::vector<std::string> names;
    for (const auto& [name, net] : loaded.nets) {
        names.push_back(name);
    }
    return names;
}

}  // namespace

TEST_CASE("machine: load errors", "[machine]") {
    Puzzle unknown = TestPattern();
    unknown.machine = "nope";
    CHECK(Errors(unknown) == Lines{"1: unknown machine 'nope'"});

    Puzzle puzzle = TestPattern();
    puzzle.stuck = {{2, "U99", "QA", LogicValue::Zero},
                    {3, "U3", "XX", LogicValue::Zero},
                    {4, "U3", "A", LogicValue::One},
                    {5, "J1", "1", LogicValue::One}};
    puzzle.cracks = {{6, Layer::Front, {0, 0}}};
    CHECK(Errors(puzzle) == Lines{"2: unknown reference 'U99'", "3: U3 (74LS161) has no pin XX",
                                  "4: U3.A (A) is not an output", "5: J1.1 (VCC) is not an output",
                                  "6: no Trace at F.Cu 0.00 0.00"});
}

TEST_CASE("machine: a crack must cut the copper", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.cracks = {{2, Layer::Front, {151'695'000, 100'080'000}}};  // H0 Trace end, on a pad
    CHECK(Errors(puzzle) == Lines{"2: crack at F.Cu 151.69 100.08 does not cut the copper"});
}

TEST_CASE("machine: no faults builds the design", "[machine]") {
    const auto machine = LoadMachine(TestPattern(), Root());
    REQUIRE(machine.has_value());
    CHECK(machine->name() == "test_pattern");
    const auto built = machine->build();
    REQUIRE(built.has_value());
    const auto design = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(design.has_value());
    CHECK(NetNames(**built) == NetNames(**design));
}

TEST_CASE("machine: swap removes only that chip's Stuck-at", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.stuck = {{2, "U3", "QA", LogicValue::Zero}};
    auto machine = LoadMachine(puzzle, Root());
    REQUIRE(machine.has_value());
    const auto tieOfV0 = [&] {
        const auto built = machine->build();
        REQUIRE(built.has_value());
        return (*built)->circuit.tieOf((*built)->nets.at("V0")).strength;
    };
    CHECK(tieOfV0() == DriveStrength::Strong);
    CHECK(machine->swap("U4"));
    CHECK(tieOfV0() == DriveStrength::Strong);
    CHECK_FALSE(machine->swap("U99"));
    CHECK(machine->swap("U3"));
    CHECK(tieOfV0() == DriveStrength::None);
}

TEST_CASE("machine: mend near and far", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.cracks = {{2, Layer::Front, kH0Crack}};
    auto machine = LoadMachine(puzzle, Root());
    REQUIRE(machine.has_value());
    CHECK(machine->build().value()->nets.contains("H0~2"));
    CHECK_FALSE(machine->mend(Layer::Front, {kH0Crack.x + 5'000'000, kH0Crack.y}));
    CHECK_FALSE(machine->mend(Layer::Back, kH0Crack));
    CHECK(machine->mend(Layer::Front, {kH0Crack.x + 900'000, kH0Crack.y}));
    CHECK_FALSE(machine->build().value()->nets.contains("H0~2"));
    CHECK_FALSE(machine->mend(Layer::Front, kH0Crack));
}

TEST_CASE("machine: HasBoard", "[machine]") {
    const std::filesystem::path netlists(BW_NETLIST_DIR);
    CHECK(HasBoard(netlists / "test_pattern.bwn"));
    CHECK(HasBoard(netlists / "counter.bwn"));
    const auto temp = std::filesystem::temp_directory_path() / "bw_has_board";
    std::filesystem::create_directories(temp);
    std::filesystem::copy_file(netlists / "test_pattern.bwn", temp / "test_pattern.bwn",
                               std::filesystem::copy_options::overwrite_existing);
    CHECK_FALSE(HasBoard(temp / "test_pattern.bwn"));
    std::filesystem::remove_all(temp);
}

TEST_CASE("machine: MachineRoot", "[machine]") {
    CHECK(MachineRoot(std::filesystem::path(BW_PUZZLE_DIR) / "dead_counter.bwp") == Root());
    CHECK(MachineRoot("/a/b/c.bwp") == "/a");
}

TEST_CASE("machine: ReadPuzzleFile + LoadMachine", "[machine]") {
    const auto file = std::filesystem::path(BW_PUZZLE_DIR) / "dead_counter.bwp";
    const auto puzzle = ReadPuzzleFile(file);
    REQUIRE(puzzle.has_value());
    CHECK(puzzle->machine == "test_pattern");
    REQUIRE(puzzle->stuck.size() == 1);
    const auto viaRead = LoadMachine(*puzzle, MachineRoot(file));
    const auto direct = LoadPuzzleFile(file);
    REQUIRE(viaRead.has_value());
    REQUIRE(direct.has_value());
    CHECK(viaRead->name() == direct->name());
    const auto built = viaRead->build();
    REQUIRE(built.has_value());
    CHECK((*built)->circuit.tieOf((*built)->nets.at("V0")).strength == DriveStrength::Strong);
    CHECK_FALSE(ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "nope.bwp").has_value());
}

TEST_CASE("machine: netlist with a Board loads fault-free", "[machine]") {
    const auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(machine.has_value());
    CHECK(machine->name() == "counter");
    CHECK(machine->board().traces.size() == machine->pristineBoard().traces.size());
    CHECK(machine->designCircuit().nets.contains("VCC"));
}

TEST_CASE("machine: Machine without VIDEO runs", "[machine]") {
    const auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(machine.has_value());
    auto loaded = machine->build();
    REQUIRE(loaded.has_value());
    Runner runner((*loaded)->circuit, {});
    runner.setSpeed(Speed::Max);
    runner.pauseAt(1_ms);
    runner.powerOn(1);
    runner.waitIdle();
    CHECK(runner.status().simTime >= 1_ms);
}

TEST_CASE("machine: crack adds a Trace to the drawn Board", "[machine]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp");
    REQUIRE(machine.has_value());
    CHECK(machine->board().traces.size() == machine->pristineBoard().traces.size() + 1);
}

TEST_CASE("machine: probeNet across a crack", "[machine]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const LoadedCircuit& loaded = **built;
    const Board& board = machine->board();

    const auto index = FindCrackTrace(machine->pristineBoard(), Layer::Front, kH0Crack);
    REQUIRE(index.has_value());
    const Trace& h0 = machine->pristineBoard().traces[*index];
    const double dx = static_cast<double>(h0.end.x - h0.start.x);
    const double dy = static_cast<double>(h0.end.y - h0.start.y);
    const double length = std::hypot(dx, dy);
    const auto along = [&](double nm) {
        return Point{kH0Crack.x + static_cast<std::int64_t>(dx / length * nm),
                     kH0Crack.y + static_cast<std::int64_t>(dy / length * nm)};
    };
    const auto before = machine->probeNet(loaded, Layer::Front, along(-1'000'000));
    const auto after = machine->probeNet(loaded, Layer::Front, along(1'000'000));
    REQUIRE(before.has_value());
    REQUIRE(after.has_value());
    CHECK(*before != *after);

    const auto qa = machine->padPosition("U1", "14");
    REQUIRE(qa.has_value());
    CHECK(machine->probeNet(loaded, Layer::Back, *qa) == PinNet(loaded, "U1", "QA"));

    const Islands islands = FindIslands(board);
    std::size_t vias = 0;
    for (std::size_t v = 0; v < board.vias.size(); ++v) {
        for (std::size_t k = 0; k < board.components.size(); ++k) {
            const auto& pads = board.components[k].pads;
            for (std::size_t p = 0; p < pads.size(); ++p) {
                if (islands.padIsland[k][p] != islands.viaIsland[v]) {
                    continue;
                }
                const auto net = PinNet(loaded, board.components[k].reference, pads[p].number);
                if (net) {
                    CHECK(machine->probeNet(loaded, Layer::Front, board.vias[v].position) == net);
                    ++vias;
                }
            }
        }
    }
    CHECK(vias > 0);

    CHECK_FALSE(machine->probeNet(loaded, Layer::Front, {0, 0}));
    std::size_t backOnly = 0;
    for (const Trace& trace : board.traces) {
        const Point mid{(trace.start.x + trace.end.x) / 2, (trace.start.y + trace.end.y) / 2};
        if (trace.layer != Layer::Back || trace.mid || !Covers(trace, mid)) {
            continue;
        }
        bool covered = false;
        for (const BoardComponent& component : board.components) {
            for (const Pad& pad : component.pads) {
                covered = covered || Covers(pad, mid);
            }
        }
        for (const Via& via : board.vias) {
            covered = covered || Covers(via, mid);
        }
        for (const Trace& front : board.traces) {
            covered = covered || (front.layer == Layer::Front && Covers(front, mid));
        }
        if (!covered) {
            CHECK(machine->probeNet(loaded, Layer::Back, mid).has_value());
            CHECK_FALSE(machine->probeNet(loaded, Layer::Front, mid));
            ++backOnly;
        }
    }
    CHECK(backOnly > 0);
}

TEST_CASE("machine: padPosition", "[machine]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp");
    REQUIRE(machine.has_value());
    const auto qa = machine->padPosition("U1", "QA");
    REQUIRE(qa.has_value());
    CHECK(machine->padPosition("U1", "14") == qa);
    bool found = false;
    for (const BoardComponent& component : machine->board().components) {
        for (const Pad& pad : component.pads) {
            if (component.reference == "U1" && pad.number == "14") {
                CHECK(pad.position == *qa);
                found = true;
            }
        }
    }
    CHECK(found);
    CHECK_FALSE(machine->padPosition("U99", "1"));
    CHECK_FALSE(machine->padPosition("U1", "XX"));

    const auto counter = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(counter.has_value());
    CHECK(counter->padPosition("R1", "1").has_value());
}

namespace {

Machine BrokenTrace() {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp");
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

bool PadsConnected(const Machine& machine, const char* refA, const char* pinA, const char* refB,
                   const char* pinB) {
    const auto a = machine.padPosition(refA, pinA);
    const auto b = machine.padPosition(refB, pinB);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    return machine.connected(Layer::Front, *a, Layer::Front, *b);
}

}  // namespace

TEST_CASE("machine: bodge wire across the crack", "[machine]") {
    Machine machine = BrokenTrace();
    CHECK_FALSE(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.addWire({"U1", "14"}, {"U13", "6"}));
    CHECK(PadsConnected(machine, "U1", "14", "U13", "6"));
    REQUIRE(machine.wires().size() == 1);
    CHECK(machine.wires()[0].a.pin == "14");
    CHECK(machine.removeWire({"U13", "6"}, {"U1", "QA"}));
    CHECK_FALSE(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK_FALSE(machine.removeWire({"U13", "6"}, {"U1", "QA"}));
}

TEST_CASE("machine: bad wires", "[machine]") {
    Machine machine = BrokenTrace();
    CHECK_FALSE(machine.addWire({"U99", "1"}, {"U13", "6"}));
    CHECK_FALSE(machine.addWire({"U1", "XX"}, {"U13", "6"}));
    CHECK_FALSE(machine.addWire({"U1", "14"}, {"U1", "QA"}));
    CHECK(machine.wires().empty());
}

TEST_CASE("machine: cut and mend", "[machine]") {
    auto loaded = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(loaded.has_value());
    Machine& machine = *loaded;
    CHECK(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.cut(Layer::Front, kH0Crack));
    CHECK_FALSE(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.cuts().size() == 1);
    CHECK_FALSE(machine.cut(Layer::Front, kH0Crack));
    CHECK_FALSE(machine.cut(Layer::Front, {0, 0}));
    CHECK(machine.mend(Layer::Front, kH0Crack));
    CHECK(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.cuts().empty());
}

TEST_CASE("machine: build with a wire", "[machine]") {
    Machine machine = BrokenTrace();
    REQUIRE(machine.addWire({"U1", "14"}, {"U13", "6"}));
    const auto built = machine.build();
    REQUIRE(built.has_value());
    CHECK(PinNet(**built, "U1", "14") == PinNet(**built, "U13", "6"));
}
