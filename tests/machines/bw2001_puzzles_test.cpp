// Machine 130 BW-2001 Puzzles (docs/machines/130-bw2001/BW2001_DESIGN.md, Puzzles): each from
// power-on, faulty, read as the player reads it (the TV, the video RAM, the self-test's rows),
// then repaired. Release-only but the first two: debug runs the Machine at about 0.004x.

#include <algorithm>
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
std::vector<std::string> SelfTest(Machine& machine, std::uint64_t seed = 1) {
    REQUIRE(machine.addWire({"J2", "DIAG"}, {"J2", "1"}));
    auto circuit = Bw2001Built(machine);
    REQUIRE(machine.removeWire({"J2", "DIAG"}, {"J2", "1"}));
    Running m(seed, std::move(circuit));
    m.runUntil(1500_ms);
    return Bw2001ScreenRows(m);
}

// The banner and READY by 1.5 s: the fault-free check after a repair.
bool Banner(Running& m) {
    Bw2001Typist typist(m);
    return typist.waitForLine(1'500'000) && typist.rows() == kBw2001Banner;
}

// The Puzzles, in BW2001.md's order.
constexpr std::array kPuzzles{
    "odd_e",        "striped_screen", "rolling_text",  "stutter",       "negative_screen",
    "short_memory", "missing_keys",   "frozen_cursor", "random_screen", "snow"};

// The Puzzle's Machine powered on with `seed`, run to `until`; the snow watch on if asked.
Running Run(const Machine& machine, std::uint64_t seed = 1, Time until = 700_ms,
            bool watchSnow = false) {
    Running m(seed, Bw2001Built(machine), watchSnow);
    m.runUntil(until);
    return m;
}

// The video RAM's row `row`, "" past the last.
std::string Row(const Running& m, std::size_t row) {
    const auto rows = Bw2001ScreenRows(m);
    return row < rows.size() ? rows[row] : "";
}

// Video RAM cells that are not spaces.
int Marks(const Running& m) {
    int marks = 0;
    for (std::size_t cell = 0; cell < kBw2001Rows * kBw2001Columns; ++cell) {
        marks += m.screen(cell) != 0x20 ? 1 : 0;
    }
    return marks;
}

// Bad zero page and Wrong chip on `seed`: the screen cleared, a few stray characters, no banner.
void CheckBlankDead(const Machine& machine, std::uint64_t seed) {
    INFO("seed " << seed);
    Running m(seed, Bw2001Built(machine));
    m.runUntil(1500_ms);
    CHECK(Marks(m) > 0);
    CHECK(Marks(m) < 100);
    CHECK(Bw2001ScreenRows(m) != kBw2001Banner);
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
        // wrong_chip's dry pin is on no Net: the builder says so (the Joint is the Fault).
        const bool dry = name == std::string("wrong_chip") && circuit->warnings.size() == 1 &&
                         Format(circuit->warnings[0]).find("2A4") != std::string::npos;
        CHECK((circuit->warnings.empty() || dry));
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

TEST_CASE("bw2001: short_memory", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("short_memory");
    {
        Running m(1, Bw2001Built(machine));
        Bw2001Typist typist(m);
        REQUIRE(typist.waitForLine(1'500'000));
        auto banner = kBw2001Banner;
        banner[2] = "2046 BYTES FREE";
        CHECK(typist.rows() == banner);
        const auto rows = SelfTest(machine);
        REQUIRE(rows.size() >= 5);
        CHECK(rows[4] == "RAM BAD 0C00 01");
    }
    REQUIRE(machine.swap("UI5"));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: missing_keys", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("missing_keys");
    {
        Running m(1, Bw2001Built(machine));
        REQUIRE(Banner(m));
        Bw2001Type(m, "QWERTYASDF");
        CHECK(Row(m, 5) == "ERTYDF");
    }
    REQUIRE(machine.mend(MissingKeysCrack().layer, MissingKeysCrack().at));
    Running m(1, Bw2001Built(machine));
    REQUIRE(Banner(m));
    Bw2001Type(m, "QWERTYASDF");
    CHECK(Row(m, 5) == "QWERTYASDF");
}

TEST_CASE("bw2001: frozen_cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    constexpr std::size_t kCursor = 5 * kBw2001Columns;  // under READY
    Machine machine = LoadPuzzle("frozen_cursor");
    {
        Running m(1, Bw2001Built(machine));
        REQUIRE(Banner(m));
        m.runUntil(m.sim->now() + 50_ms);  // the editor puts the cursor out once itself
        const std::uint8_t first = m.screen(kCursor);
        for (int i = 0; i < 6; ++i) {  // a second
            m.runUntil(m.sim->now() + 170_ms);
            CHECK(m.screen(kCursor) == first);
        }
        Bw2001Type(m, "PRINT 2+2\r");
        m.runUntil(m.sim->now() + 200_ms);
        CHECK(Bw2001ScreenRows(m) == kBw2001Banner);
    }
    REQUIRE(machine.swap("UG8"));
    Running m(1, Bw2001Built(machine));
    REQUIRE(Banner(m));
    bool blinked = false;
    const std::uint8_t first = m.screen(kCursor);
    for (int i = 0; i < 6; ++i) {
        m.runUntil(m.sim->now() + 170_ms);
        blinked = blinked || m.screen(kCursor) != first;
    }
    CHECK(blinked);
    Bw2001Type(m, "PRINT 2+2\r");
    Bw2001Typist typist(m);
    REQUIRE(typist.waitForLine());
    CHECK(Row(m, 6) == "4");
}

TEST_CASE("bw2001: random_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("random_screen");
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        Running m(seed, Bw2001Built(machine));
        m.runUntil(100_ms);
        const auto atPowerOn = Bw2001ScreenRows(m);
        m.runUntil(1000_ms);
        CHECK(Bw2001ScreenRows(m) == atPowerOn);  // nothing writes the screen
        CHECK(Bw2001ScreenRows(m) != kBw2001Banner);
        CHECK(m.sim->value(*PinNet(*m.loaded, "UF3", "RES_N")) == LogicValue::Zero);
        const auto selfTest = SelfTest(machine, seed);
        CHECK((selfTest.empty() || selfTest[0] != "BW-2001 SELF-TEST"));
    }
    REQUIRE(machine.swap("UDE9"));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: snow", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 8 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("snow");
    const auto loop = [](Running& m) {
        Bw2001Typist typist(m);
        REQUIRE(typist.waitForLine(1'500'000));
        const int before = m.snow->accesses;  // from the banner (the power-on address)
        Bw2001Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
        REQUIRE(typist.waitForLine(10'000'000));
        const auto rows = typist.rows();
        REQUIRE(rows.size() == 24);
        CHECK(rows[22] == "30");
        CHECK(rows[23] == "READY");
        return m.snow->accesses - before;
    };
    {
        Running m(1, Bw2001Built(machine), true);
        CHECK(loop(m) > 100);
        const auto rows = SelfTest(machine);
        REQUIRE(rows.size() >= 8);
        CHECK(rows[7] == "ROM E000 BAD E0");
    }
    REQUIRE(machine.swap("UH3"));
    Running m(1, Bw2001Built(machine), true);
    CHECK(loop(m) == 0);
}

TEST_CASE("bw2001: bad_zero_page", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("bad_zero_page");
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        CheckBlankDead(machine, seed);
    }
    const auto rows = SelfTest(machine);
    REQUIRE(rows.size() >= 3);
    CHECK(rows[2] == "ZERO PAGE BAD 0001 01");
    REQUIRE(machine.swap("UI8"));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: wrong_chip", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 8 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("wrong_chip");
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        CheckBlankDead(machine, seed);
    }
    auto rows = SelfTest(machine);
    REQUIRE(rows.size() >= 3);
    CHECK(rows[2] == "ZERO PAGE BAD 0001 01");
    REQUIRE(machine.swap("UI8"));  // the self-test's suspect: no better
    rows = SelfTest(machine);
    REQUIRE(rows.size() >= 3);
    CHECK(rows[2] == "ZERO PAGE BAD 0001 01");
    REQUIRE(machine.resolder({"UH8", "2A4"}));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: ghost_letter", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("ghost_letter");
    {
        Running m(1, Bw2001Built(machine));
        Bw2001Typist typist(m);
        REQUIRE(typist.waitForLine(1'500'000));
        auto banner = kBw2001Banner;
        banner[0] = "*** CW-2001 TINY BASIC ***";
        CHECK(typist.rows() == banner);
        CHECK(Bw2001TvRows(Frame(m)) == banner);
        const auto rows = SelfTest(machine);
        REQUIRE(rows.size() >= 4);
        CHECK(rows[0] == "BW-2101 SELF-TEST");  // its own title's 0 shows as 1
        CHECK(rows[3] == "VIDEO RAM BAD 8004 01");
    }
    REQUIRE(machine.swap("UC3"));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: no_clock", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("no_clock");
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        Running m(seed, Bw2001Built(machine));
        const NetId clk8 = *PinNet(*m.loaded, "UB2", "CLK");
        const NetId osc8 = *PinNet(*m.loaded, "X1", "OUT");
        m.runUntil(200_ms);
        CHECK_FALSE(m.tv->tv().isLocked());
        int clk = 0, osc = 0;
        LogicValue lastClk = m.sim->value(clk8), lastOsc = m.sim->value(osc8);
        for (int i = 0; i < 200; ++i) {              // 10 us
            m.sim->runUntil(m.sim->now() + 50'000);  // 50 ns
            clk += m.sim->value(clk8) != lastClk ? 1 : 0;
            osc += m.sim->value(osc8) != lastOsc ? 1 : 0;
            lastClk = m.sim->value(clk8);
            lastOsc = m.sim->value(osc8);
        }
        CHECK(clk == 0);
        CHECK(osc > 100);
        const TvFrame& frame = m.tv->tv().completedFrame();
        CHECK(std::ranges::all_of(frame.cells, [](float c) { return c == 0.0f; }));
    }
    REQUIRE(machine.mend(NoClockCrack().layer, NoClockCrack().at));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: joined_bits", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("joined_bits");
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        Running m(seed, Bw2001Built(machine));
        m.runUntil(100_ms);
        const auto atPowerOn = Bw2001ScreenRows(m);
        m.runUntil(1000_ms);
        CHECK(Bw2001ScreenRows(m) == atPowerOn);  // never cleared
        CHECK(Marks(m) > 500);
        CHECK(m.sim->value(*PinNet(*m.loaded, "UF3", "RES_N")) == LogicValue::One);
    }
    const auto rows = SelfTest(machine);
    CHECK((rows.empty() || rows[0] != "BW-2001 SELF-TEST"));
    REQUIRE(machine.removeBridge({"UF3", "D0"}, {"UF3", "D1"}));
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}
