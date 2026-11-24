// Repair Validation (P8c, docs/p8c-repair-validation/): no Puzzle is completed as loaded, every
// working Machine is; each Puzzle's own case checks it completed after its Repair.

#include <chrono>
#include <iostream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/machine.hpp"
#include "machine/machine_list.hpp"

using namespace bw;

namespace {

MachineList Listed() {
    MachineList list = ListMachines(test::RootDir() / "machines");
    REQUIRE(list.problems.empty());
    REQUIRE_FALSE(list.machines.empty());
    return list;
}

// The BW-2001's and BW-80's loads are left to the release build.
bool Large(const MachineEntry& machine) {
    const std::string stem = machine.netlist.stem().string();
    return stem == "bw2001" || stem == "bw80";
}

}  // namespace

TEST_CASE("repair validation: no Puzzle completed as loaded", "[machines]") {
    std::size_t checked = 0;
    for (const MachineEntry& machine : Listed().machines) {
#ifndef NDEBUG
        if (Large(machine)) {
            continue;
        }
#else
        (void)Large;
#endif
        for (const PuzzleEntry& puzzle : machine.puzzles) {
            INFO(puzzle.file.string());
            const auto loaded = LoadPuzzleFile(puzzle.file);
            REQUIRE(loaded.has_value());
            CHECK_FALSE(loaded->completed());
            ++checked;
        }
    }
    CHECK(checked > 0);
}

TEST_CASE("repair validation: every working Machine is completed", "[machines]") {
    for (const MachineEntry& machine : Listed().machines) {
        INFO(machine.netlist.string());
        const auto loaded = LoadNetlistMachine(machine.netlist);
        REQUIRE(loaded.has_value());
        CHECK(loaded->completed());
    }
}

TEST_CASE("repair validation: verdict time", "[.perf]") {
    auto machine = LoadPuzzleFile(test::PuzzlePath("odd_e"));
    REQUIRE(machine.has_value());
    REQUIRE(machine->swap("UA2"));
    // A Bodge Wire between two pins of one Net: the Copper differs, so the Wiring is worked out.
    const auto& connections = machine->designCircuit().connections;
    bool wired = false;
    for (std::size_t i = 1; i < connections.size() && !wired; ++i) {
        const Connection& a = connections[i - 1];
        const Connection& b = connections[i];
        if (a.net == b.net && a.reference != b.reference) {
            wired = machine->addWire({a.reference, std::to_string(a.pin + 1)},
                                     {b.reference, std::to_string(b.pin + 1)});
        }
    }
    REQUIRE(wired);
    const auto first = std::chrono::steady_clock::now();
    const bool completed = machine->completed();  // the golden Wiring too
    CHECK(completed);
    const auto second = std::chrono::steady_clock::now();
    constexpr int kRuns = 20;
    for (int i = 0; i < kRuns; ++i) {
        (void)machine->completed();
    }
    const auto end = std::chrono::steady_clock::now();
    using Ms = std::chrono::duration<double, std::milli>;
    std::cout << "verdict: first " << Ms(second - first).count() << " ms, then "
              << Ms(end - second).count() / kRuns << " ms (completed " << completed << ")\n";
}
