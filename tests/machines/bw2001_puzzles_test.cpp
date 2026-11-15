// Machine 130 BW-2001 Puzzles (docs/machines/130-bw2001/BW2001_DESIGN.md, Puzzles): each from
// power-on, faulty, read as the player reads it (the TV, the video RAM, the self-test's rows),
// then repaired. Release-only but the first two: debug runs the Machine at about 0.004x.

#include <array>
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
#include "netlist/diagnostic.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
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

// The Puzzles, in BW2001.md's order.
constexpr std::array kPuzzles{"odd_e", "striped_screen", "rolling_text", "stutter",
                              "negative_screen"};

// The Puzzle's Machine powered on with `seed`, run to `until`; the snow watch on if asked.
Running Run(const Machine& machine, std::uint64_t seed = 1, Time until = 700_ms,
            bool watchSnow = false) {
    Running m(seed, Bw2001Built(machine), watchSnow);
    m.runUntil(until);
    return m;
}

const TvFrame& Frame(Running& m) {
    m.runUntil(m.sim->now() + 40_ms);  // two whole frames
    return m.tv->tv().completedFrame();
}

}  // namespace

TEST_CASE("bw2001: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "bw2001");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("bw2001: every Puzzle builds", "[machines][slow]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const Machine machine = LoadPuzzle(name);
        const auto circuit = Bw2001Built(machine);
        for (const Diagnostic& d : circuit->warnings) {
            UNSCOPED_INFO(Format(d));
        }
        CHECK(circuit->warnings.empty());
    }
}

TEST_CASE("bw2001: odd_e", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("odd_e");
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kBw2001Banner);  // the video RAM is right
        const TvFrame& frame = Frame(m);
        auto e = Bw2001Glyph(0x05);
        e[2] = static_cast<std::uint8_t>(e[2] | 0x04);  // a dot in the upper half
        int es = 0;
        for (int cell = 0; cell < 5 * kBw2001Columns; ++cell) {
            const std::uint8_t code = m.screen(static_cast<std::size_t>(cell));
            INFO("cell " << cell);
            if (code == 0x05) {
                CHECK(Bw2001CellDots(frame, cell) == e);
                ++es;
            } else {
                CHECK(Bw2001CellDots(frame, cell) == Bw2001Glyph(code));
            }
        }
        CHECK(es == 4);  // BYTES FREE, READY
    }
    REQUIRE(machine.swap("UA2"));
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kBw2001Banner);
}

TEST_CASE("bw2001: striped_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("striped_screen");
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kBw2001Banner);
        const TvFrame& frame = Frame(m);
        const std::size_t cursor = 5 * kBw2001Columns;  // drawn reversed in one blink phase
        for (int cell = 0; cell < kBw2001Rows * kBw2001Columns; ++cell) {
            if (static_cast<std::size_t>(cell) == cursor) {
                continue;
            }
            INFO("cell " << cell);
            const auto dots = Bw2001CellDots(frame, cell);
            const auto glyph = Bw2001Glyph(m.screen(static_cast<std::size_t>(cell)));
            for (int line = 0; line < 8; ++line) {
                CHECK(dots[line] == (glyph[line] | 0x08));  // CD3: the fifth dot
            }
        }
    }
    REQUIRE(machine.mend(StripedScreenCrack().layer, StripedScreenCrack().at));
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kBw2001Banner);
}

TEST_CASE("bw2001: rolling_text", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.5 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("rolling_text");
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kBw2001Banner);
        for (int i = 0; i < 50; ++i) {  // a second
            m.runUntil(m.sim->now() + 20_ms);
            CHECK_FALSE(m.tv->tv().isLocked());
        }
    }
    REQUIRE(machine.mend(RollingTextCrack().layer, RollingTextCrack().at));
    Running m = Run(machine);
    CHECK(m.tv->tv().isLocked());
    CHECK(Bw2001TvRows(Frame(m)) == kBw2001Banner);
}

TEST_CASE("bw2001: stutter", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("stutter");
    {
        Running m = Run(machine);
        const auto rows = Bw2001TvRows(Frame(m));
        REQUIRE(rows.size() >= 3);
        CHECK(rows[0] == "**  WW2200  IIYYBBSSCC****");
        const TvFrame& frame = m.tv->tv().completedFrame();
        for (int cell = 0; cell < kBw2001Rows * kBw2001Columns; cell += 2) {  // drawn in pairs
            CHECK(Bw2001CellDots(frame, cell) == Bw2001CellDots(frame, cell + 1));
        }
    }
    REQUIRE(machine.swap("UD2"));
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kBw2001Banner);
}

TEST_CASE("bw2001: negative_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("negative_screen");
    const std::array<std::uint8_t, 8> lit{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::array<std::uint8_t, 8> dark{};
    const int blank = 10 * kBw2001Columns;  // a cell of an empty row
    {
        Running m = Run(machine);
        const TvFrame& frame = Frame(m);
        CHECK(Bw2001TvRows(frame) == kBw2001Banner);  // read as reversed text
        CHECK(Bw2001CellDots(frame, blank) == lit);
        CHECK(Bw2001CellDots(frame, 4) == Bw2001Glyph(0x82));  // B, reversed
    }
    REQUIRE(machine.swap("UC1"));
    Running m = Run(machine);
    const TvFrame& frame = Frame(m);
    CHECK(Bw2001TvRows(frame) == kBw2001Banner);
    CHECK(Bw2001CellDots(frame, blank) == dark);
}
