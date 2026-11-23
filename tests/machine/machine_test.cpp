#include "machine/machine.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/copper.hpp"
#include "machine/crack.hpp"
#include "parts/mos6530.hpp"
#include "parts/ram2114.hpp"
#include "runner/runner.hpp"
#include "sim/simulator.hpp"
#include "test_points.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using Lines = std::vector<std::string>;

// The Puzzle's Machine directory; a directory without its netlist for an unknown Machine.
std::filesystem::path Dir(const Puzzle& puzzle) {
    return puzzle.machine == "nope" ? test::RootDir() / "machines"
                                    : test::MachineDir(puzzle.machine);
}

Puzzle TestPattern() {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    return puzzle;
}

Puzzle LedComputer() {
    Puzzle puzzle = TestPattern();
    puzzle.machine = "led_computer";
    return puzzle;
}

Lines Errors(const Puzzle& puzzle) {
    const auto result = LoadMachine(puzzle, Dir(puzzle));
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
                    {4, "J2", "VIDEO", LogicValue::One},
                    {5, "J1", "1", LogicValue::One}};
    puzzle.cracks = {{6, {"U99", "1"}, {"U1", "QA"}, 50}};
    CHECK(Errors(puzzle) == Lines{"2: unknown reference 'U99'", "3: U3 (74LS161) has no pin XX",
                                  "4: J2.VIDEO (VIDEO) is not an output or a chip input",
                                  "5: J1.1 (VCC) is not an output or a chip input",
                                  "6: unknown reference 'U99'"});
}

TEST_CASE("machine: a chip input can be stuck, and a swap removes it", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.stuck = {{2, "U3", "A", LogicValue::One}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    const auto ownNets = [&] {
        const auto built = machine->build();
        REQUIRE(built.has_value());
        return (*built)->circuit.netCount() - (*built)->nets.size();
    };
    const auto withStuck = ownNets();
    CHECK(machine->swap("U3"));
    CHECK(ownNets() == withStuck - 1);
}

TEST_CASE("machine: joint errors", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.joints = {{2, {"U99", "1"}}, {3, {"U3", "XX"}}, {4, {"J1", "1"}}, {5, {"U3", "16"}}};
    CHECK(Errors(puzzle) == Lines{"2: unknown reference 'U99'", "3: U3 (74LS161) has no pin XX",
                                  "4: joint J1.1: chip signal pins only",
                                  "5: joint U3.16: chip signal pins only"});
}

TEST_CASE("machine: resolder removes a dry Joint, a swap does not", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.joints = {{2, {"U13", "2B"}}, {3, {"U13", "6"}}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    REQUIRE(machine->dryJoints().size() == 1);
    CHECK(machine->dryJoints()[0].reference == "U13");
    CHECK(machine->dryJoints()[0].pin == "6");
    CHECK(machine->swap("U13"));
    CHECK(machine->dryJoints().size() == 1);
    CHECK_FALSE(machine->resolder({"U99", "1"}));
    CHECK_FALSE(machine->resolder({"J1", "1"}));
    CHECK_FALSE(machine->resolder({"U13", "VCC"}));
    CHECK(machine->resolder({"U13", "5"}));  // a good Joint
    CHECK(machine->dryJoints().size() == 1);
    CHECK(machine->resolder({"U13", "2B"}));
    CHECK(machine->dryJoints().empty());
}

TEST_CASE("machine: crack errors", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.cracks = {{2, {"U1", "QA"}, {"U1", "14"}, 50},
                     {3, {"U1", "QA"}, {"U5", "A"}, 50},
                     {4, {"U1", "XX"}, {"U5", "A"}, 50}};
    CHECK(Errors(puzzle) == Lines{"2: crack U1.QA U1.14: two different pads",
                                  "3: crack U1.QA U5.A: pads not joined by Copper",
                                  "4: U1 (74LS161) has no pin XX"});
}

TEST_CASE("machine: no faults builds the design", "[machine]") {
    const auto machine = LoadMachine(TestPattern(), Dir(TestPattern()));
    REQUIRE(machine.has_value());
    CHECK(machine->name() == "test_pattern");
    const auto built = machine->build();
    REQUIRE(built.has_value());
    const auto design = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(design.has_value());
    CHECK(NetNames(**built) == NetNames(**design));
}

TEST_CASE("machine: swap removes only that chip's Stuck-at", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.stuck = {{2, "U3", "QA", LogicValue::Zero}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
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
    puzzle.cracks = {{2, {"U1", "QA"}, {"U13", "2B"}, 79}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    CHECK(machine->build().value()->nets.contains("H0~2"));
    CHECK_FALSE(machine->mend(Layer::Front, {H0Crack().at.x + 5'000'000, H0Crack().at.y}));
    CHECK_FALSE(machine->mend(Layer::Back, H0Crack().at));
    CHECK(machine->mend(Layer::Front, {H0Crack().at.x + 900'000, H0Crack().at.y}));
    CHECK_FALSE(machine->build().value()->nets.contains("H0~2"));
    CHECK_FALSE(machine->mend(H0Crack().layer, H0Crack().at));
}

TEST_CASE("machine: inject adds a Fault to a loaded Machine", "[machine]") {
    const Puzzle puzzle = TestPattern();
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    CHECK(machine->build().value()->nets.contains("H0"));
    CHECK_FALSE(machine->build().value()->nets.contains("H0~2"));
    REQUIRE(machine->inject(CrackDecl{0, {"U1", "QA"}, {"U13", "2B"}, 79}).has_value());
    CHECK(machine->cracks().size() == 1);
    CHECK(machine->build().value()->nets.contains("H0~2"));  // the board was rebuilt
    REQUIRE(machine->inject(StuckAtDecl{0, "U3", "QA", LogicValue::Zero}).has_value());
    CHECK(machine->swap("U3"));
    machine->undoRepairs();  // both are the Puzzle's now
    CHECK(machine->cracks().size() == 1);
    const auto built = machine->build();
    REQUIRE(built.has_value());
    CHECK((*built)->circuit.tieOf((*built)->nets.at("V0")).strength == DriveStrength::Strong);
    CHECK(machine->inject(StuckAtDecl{0, "U99", "QA", LogicValue::Zero}).error() ==
          "unknown reference 'U99'");
    CHECK(machine->inject(SwitchDecl{0, "U3", true}).error() == "switch U3: buttons and keys only");
}

TEST_CASE("machine: apply dispatches each Repair", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.cracks = {{2, {"U1", "QA"}, {"U13", "2B"}, 79}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    CHECK_FALSE(machine->apply(SwapRepair{"U99"}));
    CHECK(machine->apply(SwapRepair{"U3"}));
    CHECK_FALSE(machine->apply(MendRepair{Layer::Back, H0Crack().at}));
    CHECK(machine->apply(MendRepair{H0Crack().layer, H0Crack().at}));
    CHECK_FALSE(machine->build().value()->nets.contains("H0~2"));
    CHECK_FALSE(machine->apply(UnbridgeRepair{{"U1", "1"}, {"U1", "2"}}));  // no such bridge
    CHECK_FALSE(machine->apply(UnwireRepair{{"U1", "1"}, {"U1", "2"}}));    // no such wire
}

TEST_CASE("machine: HasBoard", "[machine]") {
    CHECK(HasBoard(test::NetlistPath("test_pattern")));
    CHECK(HasBoard(test::NetlistPath("counter")));
    const auto temp = std::filesystem::temp_directory_path() / "bw_has_board";
    std::filesystem::create_directories(temp);
    std::filesystem::copy_file(test::NetlistPath("test_pattern"), temp / "test_pattern.bwn",
                               std::filesystem::copy_options::overwrite_existing);
    CHECK_FALSE(HasBoard(temp / "test_pattern.bwn"));
    std::filesystem::remove_all(temp);
}

TEST_CASE("machine: MachineDir", "[machine]") {
    CHECK(MachineDir(test::PuzzlePath("dead_counter")) == test::MachineDir("test_pattern"));
    CHECK(MachineDir(test::NetlistPath("counter")) == test::MachineDir("counter"));
    CHECK(MachineDir("/a/b/c.bwp") == "/a");
    CHECK(MachineDir("/a/b/c.bwn") == "/a/b");
}

TEST_CASE("machine: ReadPuzzleFile + LoadMachine", "[machine]") {
    const auto file = test::PuzzlePath("dead_counter");
    const auto puzzle = ReadPuzzleFile(file);
    REQUIRE(puzzle.has_value());
    CHECK(puzzle->machine == "test_pattern");
    REQUIRE(puzzle->stuck.size() == 1);
    const auto viaRead = LoadMachine(*puzzle, MachineDir(file));
    const auto direct = LoadPuzzleFile(file);
    REQUIRE(viaRead.has_value());
    REQUIRE(direct.has_value());
    CHECK(viaRead->name() == direct->name());
    const auto built = viaRead->build();
    REQUIRE(built.has_value());
    CHECK((*built)->circuit.tieOf((*built)->nets.at("V0")).strength == DriveStrength::Strong);
    CHECK_FALSE(ReadPuzzleFile(test::RootDir() / "machines" / "nope.bwp").has_value());
}

TEST_CASE("machine: netlist with a Board loads fault-free", "[machine]") {
    const auto machine = LoadNetlistMachine(test::NetlistPath("counter"));
    REQUIRE(machine.has_value());
    CHECK(machine->name() == "counter");
    CHECK(machine->board().traces.size() == machine->pristineBoard().traces.size());
    CHECK(machine->designCircuit().nets.contains("VCC"));
}

TEST_CASE("machine: Machine without VIDEO runs", "[machine]") {
    const auto machine = LoadNetlistMachine(test::NetlistPath("counter"));
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
    const auto machine = LoadPuzzleFile(test::PuzzlePath("broken_trace"));
    REQUIRE(machine.has_value());
    CHECK(machine->board().traces.size() == machine->pristineBoard().traces.size() + 1);
}

TEST_CASE("machine: probeNet across a crack", "[machine]") {
    const auto machine = LoadPuzzleFile(test::PuzzlePath("broken_trace"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const LoadedCircuit& loaded = **built;
    const Board& board = machine->board();

    const auto index = FindCrackTrace(machine->pristineBoard(), H0Crack().layer, H0Crack().at);
    REQUIRE(index.has_value());
    const Trace& h0 = machine->pristineBoard().traces[*index];
    const double dx = static_cast<double>(h0.end.x - h0.start.x);
    const double dy = static_cast<double>(h0.end.y - h0.start.y);
    const double length = std::hypot(dx, dy);
    const auto along = [&](double nm) {
        return Point{H0Crack().at.x + static_cast<std::int64_t>(dx / length * nm),
                     H0Crack().at.y + static_cast<std::int64_t>(dy / length * nm)};
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

TEST_CASE("machine: probeNet on an unconnected pin", "[machine]") {
    // The heartbeat fixture: U1 RW has a pad but no copper and no netlist Net.
    const auto machine = LoadNetlistMachine(test::HeartbeatNetlistPath());
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const auto rw = machine->padPosition("U1", "34");
    REQUIRE(rw.has_value());
    const auto net = machine->probeNet(**built, Layer::Front, *rw);
    REQUIRE(net.has_value());
    CHECK(*net == PinNet(**built, "U1", "RW"));
}

TEST_CASE("machine: padPosition", "[machine]") {
    const auto machine = LoadPuzzleFile(test::PuzzlePath("broken_trace"));
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

    const auto counter = LoadNetlistMachine(test::NetlistPath("counter"));
    REQUIRE(counter.has_value());
    CHECK(counter->padPosition("R1", "1").has_value());
}

namespace {

Machine BrokenTrace() {
    auto machine = LoadPuzzleFile(test::PuzzlePath("broken_trace"));
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
    auto loaded = LoadNetlistMachine(test::NetlistPath("test_pattern"));
    REQUIRE(loaded.has_value());
    Machine& machine = *loaded;
    CHECK(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.cut(H0Crack().layer, H0Crack().at));
    CHECK_FALSE(PadsConnected(machine, "U1", "14", "U13", "6"));
    CHECK(machine.cuts().size() == 1);
    CHECK_FALSE(machine.cut(H0Crack().layer, H0Crack().at));
    CHECK_FALSE(machine.cut(Layer::Front, {0, 0}));
    CHECK(machine.mend(H0Crack().layer, H0Crack().at));
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

TEST_CASE("machine: bridge errors", "[machine]") {
    Puzzle puzzle = TestPattern();
    puzzle.bridges = {{2, {"U99", "1"}, {"U1", "8"}}, {3, {"U1", "99"}, {"U1", "8"}},
                      {4, {"U1", "7"}, {"U2", "8"}},  {5, {"J1", "1"}, {"J1", "2"}},
                      {6, {"U1", "7"}, {"U1", "9"}},  {7, {"U1", "7"}, {"U1", "ENP"}}};
    CHECK(Errors(puzzle) == Lines{"2: unknown reference 'U99'", "3: U1 (74LS161) has no pin 99",
                                  "4: bridge U1.7 U2.8: pins of one chip only",
                                  "5: bridge J1.1 J1.2: pins of one chip only",
                                  "6: bridge U1.7 U1.9: pins are not neighbours",
                                  "7: bridge U1.7 U1.ENP: pins of one chip only"});
}

TEST_CASE("machine: bridge joins two pads until removed", "[machine]") {
    auto fine = LoadMachine(TestPattern(), Dir(TestPattern()));
    REQUIRE(fine.has_value());
    CHECK_FALSE(PadsConnected(*fine, "U1", "7", "U1", "8"));

    Puzzle puzzle = TestPattern();
    puzzle.bridges = {{2, {"U1", "7"}, {"U1", "8"}}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    CHECK(machine->bridges().size() == 1);
    CHECK(PadsConnected(*machine, "U1", "7", "U1", "8"));
    CHECK(machine->removeBridge({"U1", "GND"}, {"U1", "7"}));
    CHECK(machine->bridges().empty());
    CHECK_FALSE(PadsConnected(*machine, "U1", "7", "U1", "8"));
    CHECK_FALSE(machine->removeBridge({"U1", "7"}, {"U1", "8"}));
}

TEST_CASE("machine: undoRepairs brings the Puzzle's Faults back", "[machine]") {
    SECTION("a stuck pin, swapped") {
        Puzzle puzzle = TestPattern();
        puzzle.stuck = {{2, "U3", "A", LogicValue::One}};
        auto machine = LoadMachine(puzzle, Dir(puzzle));
        REQUIRE(machine.has_value());
        const auto ownNets = [&] {
            const auto built = machine->build();
            REQUIRE(built.has_value());
            return (*built)->circuit.netCount() - (*built)->nets.size();
        };
        const auto withStuck = ownNets();
        REQUIRE(machine->swap("U3"));
        CHECK(ownNets() == withStuck - 1);
        machine->undoRepairs();
        CHECK(ownNets() == withStuck);
    }
    SECTION("a dry Joint and a bridge, a wire added") {
        Puzzle puzzle = TestPattern();
        puzzle.joints = {{2, {"U13", "2B"}}};
        puzzle.bridges = {{3, {"U1", "7"}, {"U1", "8"}}};
        auto machine = LoadMachine(puzzle, Dir(puzzle));
        REQUIRE(machine.has_value());
        const std::size_t wiresBefore = machine->board().padJoins.size();
        REQUIRE(machine->resolder({"U13", "2B"}));
        REQUIRE(machine->removeBridge({"U1", "7"}, {"U1", "8"}));
        REQUIRE(machine->addWire({"U1", "14"}, {"U13", "6"}));
        CHECK(machine->dryJoints().empty());
        CHECK_FALSE(PadsConnected(*machine, "U1", "7", "U1", "8"));

        machine->undoRepairs();
        CHECK(machine->dryJoints().size() == 1);
        CHECK(machine->bridges().size() == 1);
        CHECK(PadsConnected(*machine, "U1", "7", "U1", "8"));
        CHECK(machine->wires().empty());
        CHECK(machine->cuts().empty());
        CHECK(machine->board().padJoins.size() == wiresBefore);
    }
}

TEST_CASE("machine: placeCrack", "[machine]") {
    auto machine = LoadNetlistMachine(test::NetlistPath("test_pattern"));
    REQUIRE(machine.has_value());
    CHECK(machine->cracks().empty());
    const auto crack = machine->placeCrack({"U1", "QA"}, {"U13", "2B"}, 50);
    REQUIRE(crack.has_value());
    const Point qa = *machine->padPosition("U1", "14");
    const Point b2 = *machine->padPosition("U13", "6");
    REQUIRE(machine->connected(Layer::Front, qa, Layer::Front, b2));
    REQUIRE(machine->cut(crack->layer, crack->at));
    CHECK_FALSE(machine->connected(Layer::Front, qa, Layer::Front, b2));
    CHECK(machine->placeCrack({"U1", "QA"}, {"U1", "14"}, 50).error() ==
          "crack U1.QA U1.14: two different pads");
    CHECK(machine->placeCrack({"U1", "QA"}, {"U5", "A"}, 50).error() ==
          "crack U1.QA U5.A: pads not joined by Copper");
    CHECK(machine->placeCrack({"U1", "QA"}, {"U99", "1"}, 50).error() ==
          "crack U1.QA U99.1: unknown pad");
}

TEST_CASE("machine: memory fault errors", "[machine]") {
    Puzzle puzzle = LedComputer();
    puzzle.cells = {{2, {"U99", "1"}, 0, LogicValue::Zero},
                    {3, {"U3", "XX"}, 0, LogicValue::Zero},
                    {4, {"U5", "A"}, 0, LogicValue::Zero},
                    {5, {"R1", "1"}, std::nullopt, LogicValue::Zero},
                    {6, {"U3", "A0"}, std::nullopt, LogicValue::One},
                    {7, {"U3", "IO1"}, 0x400, LogicValue::One},
                    {8, {"U2", "O0"}, 0x800, LogicValue::One},
                    {9, {"U3", "IO1"}, 0x10, LogicValue::One, "ram"}};
    CHECK(Errors(puzzle) ==
          Lines{"2: unknown reference 'U99'", "3: U3 (2114) has no pin XX",
                "4: cell U5.A $0: RAM and ROM chips only", "5: column R1.1: RAM and ROM chips only",
                "6: column U3.A0: not a data pin", "7: cell U3.IO1 $400: U3 (2114) has 1024 words",
                "8: cell U2.O0 $800: U2 (2716) has 2048 words",
                "9: cell U3.IO1 ram $10: U3 (2114) has one memory: no name"});
}

TEST_CASE("machine: cell Faults in a chip with two memories", "[machine]") {
    const Mos6530 chip("U2", *FindMos6530Mask("002"), std::vector<std::uint8_t>(1024, 0xFF));
    const auto resolve = [&](std::optional<std::uint32_t> word, std::string memory) {
        const CellDecl decl{1, {"U2", "D3"}, word, LogicValue::One, std::move(memory)};
        return ResolveCell(decl, chip, "6530");
    };
    CHECK(resolve(0x3F, "").error() == "U2 (6530) has memories rom and ram: name one");
    CHECK(resolve(0x3F, "xyz").error() == "U2 (6530) has no memory 'xyz'");
    CHECK(resolve(0x40, "ram").error() == "U2 (6530) ram has 64 words");
    CHECK(resolve(0x400, "rom").error() == "U2 (6530) rom has 1024 words");
    const CellDecl pa{1, {"U2", "PA0"}, 0, LogicValue::One, "ram"};
    CHECK(ResolveCell(pa, chip, "6530").error() == "not a data pin");
    const auto ram = resolve(0x3F, "ram");
    REQUIRE(ram.has_value());
    CHECK(ram->reference == "U2");
    CHECK(ram->pin == Mos6530::kData[3]);
    CHECK(ram->word == std::size_t{0x3F});
    CHECK(ram->memory == 1u);
    const auto rom = resolve(std::nullopt, "rom");
    REQUIRE(rom.has_value());
    CHECK_FALSE(rom->word.has_value());
    CHECK(rom->memory == 0u);
}

TEST_CASE("machine: switch fault errors", "[machine]") {
    Puzzle puzzle = LedComputer();
    puzzle.switches = {{2, "SW9", true}, {3, "U3", false}, {4, "R1", true}};
    CHECK(Errors(puzzle) == Lines{"2: unknown reference 'SW9'",
                                  "3: switch U3: buttons and keys only",
                                  "4: switch R1: buttons and keys only"});
}

TEST_CASE("machine: a swap removes a switch Fault", "[machine]") {
    Puzzle puzzle = LedComputer();
    puzzle.switches = {{2, "SW1", true}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    const auto fault = [&] {
        const auto built = machine->build();
        REQUIRE(built.has_value());
        return SwitchOf(**built, "SW1")->fault();
    };
    CHECK(fault() == SwitchFault::StuckClosed);
    CHECK(machine->swap("SW1"));
    CHECK(fault() == SwitchFault::None);
}

TEST_CASE("machine: a swap removes the chip's memory Faults", "[machine]") {
    Puzzle puzzle = LedComputer();
    puzzle.cells = {{2, {"U3", "IO1"}, 0x10, LogicValue::One},
                    {3, {"U4", "14"}, std::nullopt, LogicValue::One}};  // 14: IO1
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    // Power-on contents: U3 word $10 bit 0, U4 bit 0 of every word; seeds differ elsewhere.
    const auto stuck = [&](std::uint64_t seed) {
        const auto built = machine->build();
        REQUIRE(built.has_value());
        Simulator sim((*built)->circuit);
        sim.powerOn(seed);
        const auto& u3 = static_cast<const Ram2114&>(*(*built)->components.at("U3"));
        const auto& u4 = static_cast<const Ram2114&>(*(*built)->components.at("U4"));
        bool cell = (u3.word(0x10) & 1) != 0;
        bool column = true;
        for (std::size_t a = 0; a < 1024; ++a) {
            column = column && (u4.word(a) & 1) != 0;
        }
        return std::pair{cell, column};
    };
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        CHECK(stuck(seed) == std::pair{true, true});
    }
    CHECK(machine->swap("U3"));
    bool cellEverLow = false;
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        const auto [cell, column] = stuck(seed);
        cellEverLow = cellEverLow || !cell;
        CHECK(column);
    }
    CHECK(cellEverLow);  // random again
}

TEST_CASE("machine: contents of a ROM through its stuck cells, clean after a swap", "[machine]") {
    Puzzle puzzle = LedComputer();
    puzzle.cells = {{2, {"U2", "O0"}, 0x10, LogicValue::One},
                    {3, {"U2", "O7"}, std::nullopt, LogicValue::Zero}};
    auto machine = LoadMachine(puzzle, Dir(puzzle));
    REQUIRE(machine.has_value());
    const auto& rom = dynamic_cast<const MemoryCells&>(
        *machine->designCircuit().components.at("U2"));  // the design: no Faults
    const auto held = machine->contents("U2");
    REQUIRE(held.has_value());
    CHECK(held->bits == 8);
    REQUIRE(held->words.size() == 2048);
    bool same = true;
    for (std::size_t w = 0; w < held->words.size(); ++w) {
        auto expected = static_cast<std::uint8_t>(rom.read(0, w) & 0x7F);  // the column
        if (w == 0x10) {
            expected |= 1;  // the cell
        }
        same = same && held->words[w] == expected;
    }
    CHECK(same);
    CHECK(machine->swap("U2"));
    const auto clean = machine->contents("U2");
    REQUIRE(clean.has_value());
    bool design = true;
    for (std::size_t w = 0; w < clean->words.size(); ++w) {
        design = design && clean->words[w] == rom.read(0, w);
    }
    CHECK(design);
    CHECK_FALSE(machine->contents("U3").has_value());   // a 2114: a RAM
    CHECK_FALSE(machine->contents("U5").has_value());   // a 74LS138
    CHECK_FALSE(machine->contents("U99").has_value());  // none
}
