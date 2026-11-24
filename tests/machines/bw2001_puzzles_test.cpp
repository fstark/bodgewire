// Machine 130 BW-2001 Puzzles (machines/130-bw2001/docs/BW2001_DESIGN.md, Puzzles): each from
// power-on on Commodore's BASIC 1, faulty, read as the player reads it (the TV, the video RAM),
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
#include "../paths.hpp"
#include "bw2001_rig.hpp"
#include "bw2001_screen.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/diagnostic.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// Commodore's banner and READY. by 1.5 s: the fault-free check after a repair.
bool Banner(Running& m) { return Bw2001WaitForRows(m, kPetBanner, 1500_ms); }

// After the banner: `typed`, what BASIC 1 answers, a blank row and READY. (its way).
std::vector<std::string> Answered(const std::string& typed, const std::string& answer) {
    auto rows = kPetBanner;
    for (const std::string& row : {typed, answer, std::string(), std::string("READY.")}) {
        rows.push_back(row);
    }
    return rows;
}

// The Puzzles, in BW2001.md's order.
constexpr std::array kPuzzles{"odd_e",           "striped_screen", "rolling_text",  "stutter",
                              "negative_screen", "short_memory",   "missing_keys",  "frozen_cursor",
                              "random_screen",   "snow",           "bad_zero_page", "wrong_chip",
                              "ghost_letter",    "garbled_text",   "no_clock",      "joined_bits"};

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

// Bad zero page and Wrong chip on `seed`: BASIC 1 never starts. Its start-up runs wild through
// memory, the screen's among it, before it clears the screen: the power-on jumble stays, under
// snow. No banner.
void CheckNeverStarts(const Machine& machine, std::uint64_t seed) {
    INFO("seed " << seed);
    Running m(seed, Bw2001Built(machine), true);
    m.runUntil(1000_ms);
    const int before = m.snow->accesses;
    m.runUntil(1500_ms);
    CHECK(m.snow->accesses - before > 1000);
    CHECK(Marks(m) > 500);
    const auto rows = Bw2001ScreenRows(m);
    CHECK(std::ranges::find(rows, "READY.") == rows.end());
}

const TvFrame& Frame(Running& m) {
    m.runUntil(m.sim->now() + 40_ms);  // two whole frames
    return m.tv->tv().completedFrame();
}

}  // namespace

TEST_CASE("bw2001: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
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
    CHECK_FALSE(machine.completed());
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kPetBanner);  // the video RAM is right
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
        CHECK(es == 5);  // COMMODORE, BYTES FREE, READY.
    }
    REQUIRE(machine.swap("UA2"));
    CHECK(machine.completed());
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kPetBanner);
}

TEST_CASE("bw2001: striped_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("striped_screen");
    CHECK_FALSE(machine.completed());
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kPetBanner);
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
    CHECK(machine.completed());
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kPetBanner);
}

TEST_CASE("bw2001: rolling_text", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.5 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("rolling_text");
    CHECK_FALSE(machine.completed());
    {
        Running m = Run(machine);
        CHECK(Bw2001ScreenRows(m) == kPetBanner);
        for (int i = 0; i < 50; ++i) {  // a second
            m.runUntil(m.sim->now() + 20_ms);
            CHECK_FALSE(m.tv->tv().isLocked());
        }
    }
    REQUIRE(machine.mend(RollingTextCrack().layer, RollingTextCrack().at));
    CHECK(machine.completed());
    Running m = Run(machine);
    CHECK(m.tv->tv().isLocked());
    CHECK(Bw2001TvRows(Frame(m)) == kPetBanner);
}

TEST_CASE("bw2001: stutter", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("stutter");
    CHECK_FALSE(machine.completed());
    {
        Running m = Run(machine);
        const auto rows = Bw2001TvRows(Frame(m));
        REQUIRE(rows.size() >= 3);
        CHECK(rows[0] == "**  OOMMDDRR  AAII  ****");
        const TvFrame& frame = m.tv->tv().completedFrame();
        for (int cell = 0; cell < kBw2001Rows * kBw2001Columns; cell += 2) {  // drawn in pairs
            CHECK(Bw2001CellDots(frame, cell) == Bw2001CellDots(frame, cell + 1));
        }
    }
    REQUIRE(machine.swap("UD2"));
    CHECK(machine.completed());
    Running m = Run(machine);
    CHECK(Bw2001TvRows(Frame(m)) == kPetBanner);
}

TEST_CASE("bw2001: negative_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("negative_screen");
    CHECK_FALSE(machine.completed());
    const std::array<std::uint8_t, 8> lit{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::array<std::uint8_t, 8> dark{};
    const int blank = 10 * kBw2001Columns;  // a cell of an empty row
    {
        Running m = Run(machine);
        const TvFrame& frame = Frame(m);
        CHECK(Bw2001TvRows(frame) == kPetBanner);  // read as reversed text
        CHECK(Bw2001CellDots(frame, blank) == lit);
        CHECK(Bw2001CellDots(frame, 4) == Bw2001Glyph(0x83));  // C, reversed
    }
    REQUIRE(machine.swap("UC1"));
    CHECK(machine.completed());
    Running m = Run(machine);
    const TvFrame& frame = Frame(m);
    CHECK(Bw2001TvRows(frame) == kPetBanner);
    CHECK(Bw2001CellDots(frame, blank) == dark);
}

TEST_CASE("bw2001: short_memory", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("short_memory");
    CHECK_FALSE(machine.completed());
    {
        Running m(1, Bw2001Built(machine));
        auto banner = kPetBanner;
        banner[2] = " 2047 BYTES FREE";
        CHECK(Bw2001WaitForRows(m, banner, 1500_ms));
        CHECK(m.ram(0x0C00) % 2 == 1);  // the bit at the first word of UI5
    }
    REQUIRE(machine.swap("UI5"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: missing_keys", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("missing_keys");
    CHECK_FALSE(machine.completed());
    {
        Running m(1, Bw2001Built(machine));
        REQUIRE(Banner(m));
        Bw2001Type(m, "QWERTYASDF");
        CHECK(Row(m, 5) == "ERTYDF");
    }
    REQUIRE(machine.mend(MissingKeysCrack().layer, MissingKeysCrack().at));
    CHECK(machine.completed());
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
    CHECK_FALSE(machine.completed());
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
        CHECK(Bw2001ScreenRows(m) == kPetBanner);
    }
    REQUIRE(machine.swap("UG8"));
    CHECK(machine.completed());
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
    CHECK(Bw2001WaitForRows(m, Answered("PRINT 2+2", " 4"), 500_ms));
}

TEST_CASE("bw2001: random_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("random_screen");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        Running m(seed, Bw2001Built(machine));
        m.runUntil(100_ms);
        const auto atPowerOn = Bw2001ScreenRows(m);
        m.runUntil(1000_ms);
        CHECK(Bw2001ScreenRows(m) == atPowerOn);  // nothing writes the screen
        CHECK(Bw2001ScreenRows(m) != kPetBanner);
        CHECK(m.sim->value(*PinNet(*m.loaded, "UF3", "RES_N")) == LogicValue::Zero);
    }
    REQUIRE(machine.swap("UDE9"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: snow", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 8 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("snow");
    CHECK_FALSE(machine.completed());
    const auto loop = [](Running& m) {
        REQUIRE(Banner(m));
        const int before = m.snow->accesses;  // from the banner (the power-on address)
        Bw2001Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r");
        m.runUntil(m.sim->now() + 6000_ms);
        const auto rows = Bw2001ScreenRows(m);
        REQUIRE(rows.size() >= 3);
        CHECK(rows[rows.size() - 3] == " 30");
        CHECK(rows.back() == "READY.");
        return m.snow->accesses - before;
    };
    // BASIC 1 blanks the picture while it scrolls (BLANK_TV): only the printing shows snow, and a
    // working Machine shows a few specks at the scrolls' edges (10 here; 53 with the fault).
    {
        Running m(1, Bw2001Built(machine), true);
        CHECK(loop(m) > 40);
        // The Net is fine: VIDEO_ON still toggles at the VIA's leg (the retrace interrupt too).
        CHECK(m.sim->value(*PinNet(*m.loaded, "UA5", "PB5")) != LogicValue::X);
    }
    REQUIRE(machine.swap("UA5"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine), true);
    CHECK(loop(m) < 20);
}

TEST_CASE("bw2001: bad_zero_page", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("bad_zero_page");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        CheckNeverStarts(machine, seed);
    }
    REQUIRE(machine.swap("UI8"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: wrong_chip", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 8 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("wrong_chip");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        CheckNeverStarts(machine, seed);
    }
    REQUIRE(machine.swap("UI8"));  // Bad zero page's chip: no better
    CHECK_FALSE(machine.completed());
    CheckNeverStarts(machine, 1);
    REQUIRE(machine.resolder({"UH8", "2A4"}));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: ghost_letter", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("ghost_letter");
    CHECK_FALSE(machine.completed());
    {
        Running m(1, Bw2001Built(machine));
        auto banner = kPetBanner;
        banner[0] = "*** COMMODORG BASIC ***";
        CHECK(Bw2001WaitForRows(m, banner, 1500_ms));
        CHECK(Bw2001TvRows(Frame(m)) == banner);
    }
    REQUIRE(machine.swap("UC3"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: garbled_text", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("garbled_text");
    CHECK_FALSE(machine.completed());
    {
        Running m(1, Bw2001Built(machine));
        m.runUntil(1000_ms);
        const auto rows = Bw2001TvRows(Frame(m));
        REQUIRE(rows.size() == kBw2001Rows);  // every row full of !
        CHECK(rows[0] == "+++!COMMOEOSE!CASIC!+++!!!!!!!!!!!!!!!!!");
        CHECK(rows[2] == "!7177!CYUES!GSEE!!!!!!!!!!!!!!!!!!!!!!!!");
        for (std::size_t cell = 0; cell < kBw2001Rows * kBw2001Columns; ++cell) {
            CHECK((m.screen(cell) & 1) == 1);
        }
    }
    REQUIRE(machine.swap("UC3"));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: no_clock", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("no_clock");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}

TEST_CASE("bw2001: joined_bits", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("joined_bits");
    CHECK_FALSE(machine.completed());
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
    REQUIRE(machine.removeBridge({"UF3", "D0"}, {"UF3", "D1"}));
    CHECK(machine.completed());
    Running m(1, Bw2001Built(machine));
    CHECK(Banner(m));
}
