#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../machine/test_points.hpp"
#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

struct Seen {
    std::vector<float> frame;
    std::vector<std::string> lamps;
};

// Completed TV frame at 60 ms (no lock needed: same seed, same Circuit, same frame) and lamps.
Seen Watch(LoadedCircuit& loaded, std::uint64_t seed, const std::vector<std::string>& probes = {}) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, *ConnectorNet(loaded, "VIDEO", "VIDEO"),
               *ConnectorNet(loaded, "VIDEO", "SYNC_N"));
    std::vector<LogicProbe> lamps;
    for (const std::string& probe : probes) {
        const std::size_t dot = probe.find('.');
        const auto net = PinNet(loaded, probe.substr(0, dot), probe.substr(dot + 1));
        REQUIRE(net.has_value());
        lamps.emplace_back(sim, *net);
    }
    while (sim.now() < 60_ms) {
        sim.runFor(1_ms);
        tv.update();
        for (LogicProbe& probe : lamps) {
            probe.update();
        }
        sim.trimRecordings();
    }
    REQUIRE(tv.tv().completedFrames() >= 1);
    Seen seen{tv.tv().completedFrame().cells, {}};
    for (const LogicProbe& probe : lamps) {
        seen.lamps.push_back(FormatLamps(probe.lamps()));
    }
    return seen;
}

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

std::vector<float> Frame(const Machine& machine, std::uint64_t seed) {
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    return Watch(**loaded, seed).frame;
}

std::vector<float> FaultFree(std::uint64_t seed) {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    auto machine = LoadMachine(puzzle, std::filesystem::path(BW_PUZZLE_DIR).parent_path());
    REQUIRE(machine.has_value());
    return Frame(*machine, seed);
}

}  // namespace

TEST_CASE("faults: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"broken_trace", "dead_counter", "stuck_gate", "dead_rail",
                             "line_bridge", "unpowered_chip", "sync_bridge", "zips"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "test_pattern");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("faults: fault-free Machine on its Board is the netlist", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    auto netlist = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(netlist.has_value());
    CHECK(FaultFree(seed) == Watch(**netlist, seed).frame);
}

TEST_CASE("faults: dead counter output", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    const auto good = FaultFree(seed);
    Machine machine = LoadPuzzle("dead_counter");
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    const Seen seen = Watch(**loaded, seed, {"U3.QA"});
    CHECK(seen.frame != good);
    CHECK(seen.lamps == std::vector<std::string>{"LOW"});

    Machine wrong = LoadPuzzle("dead_counter");
    REQUIRE(wrong.swap("U4"));
    CHECK(Frame(wrong, seed) != good);
    REQUIRE(machine.swap("U3"));
    CHECK(Frame(machine, seed) == good);
}

TEST_CASE("faults: stuck gate output", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    const auto good = FaultFree(seed);
    Machine machine = LoadPuzzle("stuck_gate");
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    const Seen seen = Watch(**loaded, seed, {"U12.4Y"});
    CHECK(seen.frame != good);
    CHECK(seen.lamps == std::vector<std::string>{"HIGH"});

    Machine wrong = LoadPuzzle("stuck_gate");
    REQUIRE(wrong.swap("U13"));
    CHECK(Frame(wrong, seed) != good);
    REQUIRE(machine.swap("U12"));
    CHECK(Frame(machine, seed) == good);
}

TEST_CASE("faults: broken trace", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    const auto good = FaultFree(seed);
    Machine machine = LoadPuzzle("broken_trace");
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    const Seen seen = Watch(**loaded, seed, {"U13.2B", "U1.QA"});
    CHECK(seen.frame != good);
    REQUIRE(seen.lamps.size() == 2);
    CHECK(seen.lamps[0] == "-");
    CHECK(seen.lamps[1].ends_with("PULSE"));

    Machine wrong = LoadPuzzle("broken_trace");
    CHECK_FALSE(wrong.mend(Layer::Front, {H0Crack().at.x + 5'000'000, H0Crack().at.y}));
    CHECK(Frame(wrong, seed) != good);
    REQUIRE(machine.mend(H0Crack().layer, H0Crack().at));
    CHECK(Frame(machine, seed) == good);
}

TEST_CASE("faults: zips", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    const auto good = FaultFree(seed);
    Machine machine = LoadPuzzle("zips");
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    const NetId enable = *PinNet(**loaded, "U3", "ENT");
    CHECK(enable == *PinNet(**loaded, "U3", "ENP"));
    CHECK(enable != *PinNet(**loaded, "U9", "3Y"));
    const Seen seen = Watch(**loaded, seed, {"U3.ENT", "U9.3Y"});
    CHECK(seen.frame != good);
    REQUIRE(seen.lamps.size() == 2);
    CHECK(seen.lamps[0] == "-");
    CHECK(seen.lamps[1].ends_with("PULSE"));

    REQUIRE(machine.mend(ZipsCrack().layer, ZipsCrack().at));
    CHECK(Frame(machine, seed) == good);
}
