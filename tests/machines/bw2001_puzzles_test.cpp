// Machine 130 BW-2001 Puzzles (docs/machines/130-bw2001/BW2001_DESIGN.md, Puzzles): each from
// power-on, faulty, read as the player reads it (the TV, the video RAM, the self-test's rows),
// then repaired. Release-only but the first two: debug runs the Machine at about 0.004x.

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "bw2001_harness.hpp"
#include "bw2001_rig.hpp"
#include "bw2001_screen.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

[[maybe_unused]] Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// The self-test by the user manual's wire (user port DIAG to a ground pin of J2), from power-on
// with `seed`: the video RAM's rows at 1.5 s (a working Machine reaches KEY by then).
[[maybe_unused]] std::vector<std::string> SelfTest(Machine& machine, std::uint64_t seed = 1) {
    REQUIRE(machine.addWire({"J2", "DIAG"}, {"J2", "1"}));
    auto circuit = Bw2001Built(machine);
    REQUIRE(machine.removeWire({"J2", "DIAG"}, {"J2", "1"}));
    Running m(seed, std::move(circuit));
    m.runUntil(1500_ms);
    return Bw2001ScreenRows(m);
}

// The banner and READY by 1.5 s: the fault-free check after a repair.
[[maybe_unused]] bool Banner(Running& m) {
    Bw2001Typist typist(m);
    return typist.waitForLine(1'500'000) && typist.rows() == kBw2001Banner;
}

}  // namespace

