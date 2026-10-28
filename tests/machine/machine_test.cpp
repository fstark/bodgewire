#include "machine/machine.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

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
