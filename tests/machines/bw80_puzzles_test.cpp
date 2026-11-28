// Machine 140 BW-80 Puzzles (machines/140-bw80/docs/BW80_DESIGN.md, Puzzles): each from
// power-on, faulty, read as the player reads it (the TV against the page the display file
// predicts, the display file in the two 2114s, the probe's clue), then repaired. Release-only
// but the first two: debug runs the Machine at about 0.004x.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "bw80_rig.hpp"
#include "bw80_screen.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/diagnostic.hpp"
#include "parts/ram2114.hpp"
#include "sim/logic.hpp"
#include "tv_page.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// The Puzzles, in BW80.md's order.
constexpr std::array kPuzzles{
    "q_everywhere",      "doubled_lines",    "bottom_halves",  "wrong_letters",
    "quotes_for_spaces", "plain_cursor",     "blank_page",     "eight_keys",
    "same_eight_keys",   "no_print",         "rolling_screen", "torn_picture",
    "still_starting",    "vertical_stripes", "white_screen",   "stray_cursor"};

using CodeMap = std::function<unsigned(unsigned)>;
const CodeMap kSameCode = [](unsigned code) { return code; };

// The page the display file predicts through `glyphs`, each code through `map` (the inverse
// bit included). The cells past a row's end are blank whatever the chain does to a space (no
// code is loaded there).
PageDots PageOf(const LoadedCircuit& loaded, std::span<const std::uint8_t> glyphs,
                const CodeMap& map = kSameCode) {
    const auto rows = Bw80DisplayCodes(loaded);
    const auto has = [&](int row, int column) {
        const auto r = static_cast<std::size_t>(row), c = static_cast<std::size_t>(column);
        return r < rows.size() && c < rows[r].size();
    };
    PageDots page = ExpectedPage(glyphs, kBw80TvColumns, kBw80TvRows, [&](int row, int column) {
        const auto r = static_cast<std::size_t>(row), c = static_cast<std::size_t>(column);
        return map(has(row, column) ? rows[r][c] : 0);
    });
    for (int row = 0; row < kBw80TvRows; ++row) {
        for (int column = 0; column < kBw80TvColumns; ++column) {
            if (!has(row, column)) {
                for (int line = 0; line < 8; ++line) {
                    auto& dots = page[static_cast<std::size_t>(row * 8 + line)];
                    for (int dot = 0; dot < 8; ++dot) {
                        dots[static_cast<std::size_t>(column * 8 + dot)] = false;
                    }
                }
            }
        }
    }
    return page;
}

// The page with the load gate always open (Q everywhere): each row's codes, then its HALT ($76,
// drawn as $36, Q), then the byte the halted Z80 fetches again and again after the HALT (the next
// row's first code, or what follows the display file), to the row's end.
PageDots HaltFillPage(const LoadedCircuit& loaded) {
    const auto rows = Bw80DisplayCodes(loaded);
    auto address =
        static_cast<std::uint16_t>(Bw80Ram(loaded, 0x400C) | Bw80Ram(loaded, 0x400D) << 8);
    address = static_cast<std::uint16_t>(address + 1);  // the top margin's HALT
    std::vector<std::uint8_t> after;                    // the byte after each row's HALT
    for (const auto& codes : rows) {
        address = static_cast<std::uint16_t>(address + codes.size() + 1);
        after.push_back(Bw80Ram(loaded, address));
    }
    return ExpectedPage(Bw80Glyphs(), kBw80TvColumns, kBw80TvRows, [&](int row, int column) {
        const auto r = static_cast<std::size_t>(row), c = static_cast<std::size_t>(column);
        if (r >= rows.size()) {
            return 0x76u;
        }
        if (c < rows[r].size()) {
            return unsigned{rows[r][c]};
        }
        return c == rows[r].size() ? 0x76u : unsigned{after[r]};
    });
}

// The fault-free check: from power-on, the K cursor screen locked by a second, as the display
// file predicts (the inverse K at the bottom left, nothing else in the picture).
void CheckKScreen(const Machine& machine) {
    const auto loaded = Bw80Built(machine);
    const Bw80Frames f = Bw80LockedFrames(*loaded, 1, 3, 1000_ms);
    REQUIRE(f.frames.size() == 3);
    Bw80CheckSynced(f);
    CHECK(Bw80DisplayRows(*loaded)[kBw80Rows - 1] == "k");
    CHECK(FindPage(f.frames[2], PageOf(*loaded, Bw80Glyphs())).has_value());
}

// A line with a digit, a keyword, a string and spaces, listed: what the picture Puzzles show.
constexpr std::u32string_view kListing = U"10 PRINT \"HELLO 0123\"\nLIST\n";

// The Machine from power-on with `text` typed after the K screen, then a locked frame of what
// it shows (the display file's row 0 holds the listing's line).
struct Typed {
    std::unique_ptr<LoadedCircuit> loaded;
    TvFrame frame;
};

Typed TypeAndLook(const Machine& machine, std::u32string_view text = kListing) {
    Typed t{Bw80Built(machine), {}};
    Bw80Run run(*t.loaded, 1);
    Bw80Type(run, *t.loaded, text, 400_ms, 300_ms);
    const auto rows = Bw80DisplayRows(*t.loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (!rows[r].empty()) {
            UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
        }
    }
    const Bw80Frames f = run.lockedFrames(2, run.sim.now() + 600_ms);
    REQUIRE(f.frames.size() == 2);
    t.frame = f.frames[1];
    return t;
}

// The display file's rows after `text` typed on a fresh Machine (the K screen by 400 ms).
std::vector<std::string> TypedRows(const Machine& machine, std::u32string_view text) {
    const auto loaded = Bw80Built(machine);
    Bw80Run run(*loaded, 1);
    Bw80Type(run, *loaded, text, 400_ms, 300_ms);
    const auto rows = Bw80DisplayRows(*loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (!rows[r].empty()) {
            UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
        }
    }
    REQUIRE(rows.size() == kBw80Rows);
    return rows;
}

// The sync Puzzles: the display file holds the K cursor by 500 ms while the TV never locks
// over the next second.
void CheckUnlocked(const LoadedCircuit& loaded, Bw80Run& run) {
    run.runTo(500_ms);
    CHECK(Bw80DisplayRows(loaded)[kBw80Rows - 1] == "k");
    for (int i = 0; i < 50; ++i) {
        run.runTo(run.sim.now() + 20_ms);
        CHECK_FALSE(run.tv->tv().isLocked());
    }
}

// Changes of `net` over 10 us in 50 ns steps: a clock or select that moves, or one that is
// still past a crack or a stuck gate.
int Toggles(Bw80Run& run, NetId net) {
    int toggles = 0;
    LogicValue last = run.sim.value(net);
    for (int i = 0; i < 200; ++i) {
        run.sim.runUntil(run.sim.now() + 50'000);  // 50 ns
        toggles += run.sim.value(net) != last ? 1 : 0;
        last = run.sim.value(net);
    }
    return toggles;
}

NetId NetAt(const LoadedCircuit& loaded, const char* reference, const char* pin) {
    const auto net = PinNet(loaded, reference, pin);
    REQUIRE(net.has_value());
    return *net;
}

// The picture is `page` and not the fault-free one.
void CheckPicture(const Typed& t, const PageDots& page) {
    CHECK(FindPage(t.frame, page).has_value());
    CHECK_FALSE(FindPage(t.frame, PageOf(*t.loaded, Bw80Glyphs())).has_value());
}

}  // namespace

TEST_CASE("bw80: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "bw80");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("bw80: every Puzzle builds", "[machines][slow]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const Machine machine = LoadPuzzle(name);
        const auto circuit = Bw80Built(machine);
        for (const Diagnostic& d : circuit->warnings) {
            UNSCOPED_INFO(Format(d));
        }
        // quotes_for_spaces's dry pin is on no Net: the builder says so (the Joint is the Fault).
        const bool dry = name == std::string("quotes_for_spaces") &&
                         circuit->warnings.size() == 1 &&
                         Format(circuit->warnings[0]).find("3Q") != std::string::npos;
        CHECK((circuit->warnings.empty() || dry));
    }
}

TEST_CASE("bw80: q_everywhere", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // The HALT ending each row loads its glyph, $36 (Q), then the byte the halted Z80 fetches
    // after it repeats to the row's end: mostly the next row's HALT, Q again.
    Machine machine = LoadPuzzle("q_everywhere");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CheckPicture(t, HaltFillPage(*t.loaded));
    }
    REQUIRE(machine.swap("IC16"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: doubled_lines", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("doubled_lines");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsRows(Bw80Glyphs(), {0, 0, 2, 2, 4, 4, 6, 6})));
    }
    REQUIRE(machine.swap("IC21"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: bottom_halves", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("bottom_halves");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CHECK(*PinNet(*t.loaded, "IC8", "4A") != *PinNet(*t.loaded, "IC21", "QD"));
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsRows(Bw80Glyphs(), {4, 5, 6, 7, 4, 5, 6, 7})));
    }
    REQUIRE(machine.mend(BottomHalvesCrack().layer, BottomHalvesCrack().at));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: wrong_letters", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // CH1 and CH2 bridged: where they differ both read low (TTL contention), so the glyph is
    // the code's with bits 1 and 2 cleared unless both are set.
    Machine machine = LoadPuzzle("wrong_letters");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CHECK(Bw80DisplayRows(*t.loaded)[0].find("PRINT \"HELLO 0123\"") != std::string::npos);
        const auto bridged = [](unsigned code) {
            return (code & 0x06) == 0x06 ? code : (code & ~0x06u);
        };
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsMapped(Bw80Glyphs(), bridged)));
    }
    REQUIRE(machine.removeBridge({"IC5", "15"}, {"IC5", "16"}));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: quotes_for_spaces", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // CH0's dry Joint: IC8 reads it high, so every even code is drawn as the next one.
    Machine machine = LoadPuzzle("quotes_for_spaces");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CHECK(*PinNet(*t.loaded, "IC5", "3Q") != *PinNet(*t.loaded, "IC8", "1A"));
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsMapped(Bw80Glyphs(), [](unsigned code) {
                         return code | 1;
                     })));
    }
    REQUIRE(machine.resolder({"IC5", "6"}));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: plain_cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    // The inverse latch stuck: the cursor's $B0 drawn as a plain K.
    Machine machine = LoadPuzzle("plain_cursor");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        const Bw80Frames f = Bw80LockedFrames(*loaded, 1, 3, 1000_ms);
        REQUIRE(f.frames.size() == 3);
        CHECK(Bw80DisplayRows(*loaded)[kBw80Rows - 1] == "k");
        const auto plain = [](unsigned code) { return code & 0x7F; };
        CHECK(FindPage(f.frames[2], PageOf(*loaded, Bw80Glyphs(), plain)).has_value());
        CHECK_FALSE(FindPage(f.frames[2], PageOf(*loaded, Bw80Glyphs())).has_value());
    }
    REQUIRE(machine.swap("IC12"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: blank_page", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // The shift register never shifts: no dots, only the inverse latch's black square at the
    // cursor; the display file holds the listing all the same.
    Machine machine = LoadPuzzle("blank_page");
    CHECK_FALSE(machine.completed());
    {
        const Typed t = TypeAndLook(machine);
        CHECK(Bw80DisplayRows(*t.loaded)[0].find("PRINT \"HELLO 0123\"") != std::string::npos);
        const std::vector<std::uint8_t> blank(Bw80Glyphs().size(), 0);
        CheckPicture(t, PageOf(*t.loaded, blank));
        CHECK(InkCount(t.frame) > 0);
    }
    REQUIRE(machine.swap("IC9"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: eight_keys", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 4 s simulated: runs in the release build");
#endif
    // KB2 cut at the buffer: its input floats high, the column never reads pressed (3 among
    // them); R15 and the connector keep the rest of the Net.
    Machine machine = LoadPuzzle("eight_keys");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        CHECK(*PinNet(*loaded, "J3", "KB2") != *PinNet(*loaded, "IC10", "A5"));
        {
            Bw80Run run(*loaded, 1);  // the column at the connector keeps its pull-up
            run.runTo(10_ms);
            CHECK(run.sim.value(*PinNet(*loaded, "J3", "KB2")) == LogicValue::One);
        }
        const auto rows = TypedRows(machine, U"PRINT 12345\n");
        CHECK(rows[0] == "1245");
        CHECK(rows[kBw80Rows - 1] == "0/-2");
    }
    REQUIRE(machine.mend(EightKeysCrack().layer, EightKeysCrack().at));
    CHECK(machine.completed());
    CHECK(TypedRows(machine, U"PRINT 12345\n")[0] == "12345");
}

TEST_CASE("bw80: same_eight_keys", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 4 s simulated: runs in the release build");
#endif
    // The same column dead, but inside the buffer: the Net is whole and there is nothing to mend.
    Machine machine = LoadPuzzle("same_eight_keys");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        CHECK(machine.cracks().empty());
        CHECK(*PinNet(*loaded, "J3", "KB2") == *PinNet(*loaded, "IC10", "A5"));
        const auto rows = TypedRows(machine, U"PRINT 12345\n");
        CHECK(rows[0] == "1245");
        CHECK(rows[kBw80Rows - 1] == "0/-2");
    }
    REQUIRE(machine.swap("IC10"));
    CHECK(machine.completed());
    CHECK(TypedRows(machine, U"PRINT 12345\n")[0] == "12345");
}

TEST_CASE("bw80: no_print", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 4 s simulated: runs in the release build");
#endif
    // The keyboard's row A13 cut off at the connector: the O key (PRINT) does nothing, so NEW
    // LINE leaves 2+2 in the edit line with the syntax error marker; the CPU keeps its A13.
    Machine machine = LoadPuzzle("no_print");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        CHECK(*PinNet(*loaded, "J3", "A13") != *PinNet(*loaded, "IC1", "A13"));
        CHECK(*PinNet(*loaded, "J4", "A13") == *PinNet(*loaded, "IC1", "A13"));
        const auto rows = TypedRows(machine, U"PRINT 2+2\n");
        CHECK(rows[0] != "4");
        CHECK(rows[kBw80Rows - 1] == "2+s2l");  // the error marker and the L cursor
    }
    REQUIRE(machine.mend(NoPrintCrack().layer, NoPrintCrack().at));
    CHECK(machine.completed());
    const auto rows = TypedRows(machine, U"PRINT 2+2\n");
    CHECK(rows[0] == "4");
    CHECK(rows[kBw80Rows - 1] == "0/-2");
}

TEST_CASE("bw80: rolling_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2.5 s simulated: runs in the release build");
#endif
    // KBD_N cut just before the vertical sync latch: never reset, no vertical pulse; the
    // keyboard buffer keeps its enable.
    Machine machine = LoadPuzzle("rolling_screen");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        CHECK(*PinNet(*loaded, "IC10", "G1_N") == *PinNet(*loaded, "IC17", "4Y"));
        CHECK(*PinNet(*loaded, "IC11", "4B") != *PinNet(*loaded, "IC17", "4Y"));
        Bw80Run run(*loaded, 1);
        CheckUnlocked(*loaded, run);
    }
    REQUIRE(machine.mend(RollingScreenCrack().layer, RollingScreenCrack().at));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: torn_picture", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 2.5 s simulated: runs in the release build");
#endif
    // The sync latch's output stuck: no horizontal pulse is ever armed.
    Machine machine = LoadPuzzle("torn_picture");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        Bw80Run run(*loaded, 1);
        CheckUnlocked(*loaded, run);
    }
    REQUIRE(machine.swap("IC18"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: still_starting", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    // Bit 2 of every RAM byte stuck low: the ROM's clear-and-check of its RAM never ends, and
    // the screen stays blank and unlocked (the fault-free K screen locks by 450 ms): the ROM's
    // own code never loads a glyph, so only the TV's unblanked sync shows.
    Machine machine = LoadPuzzle("still_starting");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        const auto loaded = Bw80Built(machine);
        Bw80Run run(*loaded, seed);
        run.runTo(700_ms);
        CHECK_FALSE(run.tv->tv().isLocked());
        CHECK(InkCount(run.tv->tv().completedFrame()) < 2000);
        const auto* ram = dynamic_cast<const Ram2114*>(loaded->components.at("IC3"));
        REQUIRE(ram != nullptr);
        int set = 0;
        for (std::size_t a = 0; a < 1024; ++a) {
            set += (ram->word(a) & 8) != 0 ? 1 : 0;  // IO4, bit 3 of the 4-bit word
        }
        CHECK(set == 0);
    }
    REQUIRE(machine.swap("IC3"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: vertical_stripes", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.5 s simulated: runs in the release build");
#endif
    // ROM_CS1 cut: the select moves at the multiplexer and sits high at the ROM.
    Machine machine = LoadPuzzle("vertical_stripes");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        const auto loaded = Bw80Built(machine);
        Bw80Run run(*loaded, seed);
        run.runTo(300_ms);
        CHECK_FALSE(run.tv->tv().isLocked());
        CHECK(Toggles(run, NetAt(*loaded, "IC6", "2Y")) > 0);
        CHECK(Toggles(run, NetAt(*loaded, "IC2", "CS1")) == 0);
        CHECK(run.sim.value(NetAt(*loaded, "IC2", "CS1")) == LogicValue::One);
    }
    REQUIRE(machine.mend(VerticalStripesCrack().layer, VerticalStripesCrack().at));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: white_screen", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1.5 s simulated: runs in the release build");
#endif
    // A NOP gate stuck low: D3 held low on the CPU's side while the clock runs (the memory
    // side, MD3, may sit still too once the CPU is stuck: seed 1).
    Machine machine = LoadPuzzle("white_screen");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        const auto loaded = Bw80Built(machine);
        Bw80Run run(*loaded, seed);
        run.runTo(300_ms);
        CHECK_FALSE(run.tv->tv().isLocked());
        CHECK(InkCount(run.tv->tv().completedFrame()) < 1500);  // the frame's edge lines only
        CHECK(Toggles(run, NetAt(*loaded, "IC1", "CLK")) > 0);
        CHECK(Toggles(run, NetAt(*loaded, "IC1", "D3")) == 0);
        CHECK(run.sim.value(NetAt(*loaded, "IC1", "D3")) == LogicValue::Zero);
    }
    REQUIRE(machine.swap("IC14"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}

TEST_CASE("bw80: stray_cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 3 s simulated: runs in the release build");
#endif
    // X1 at 3.25 MHz: the processor, which draws the picture, at half speed; every line and
    // frame twice as long: the display file holds the K cursor by a second (500 ms
    // fault-free), and the TV never locks. Seed 1 only: its frames are the same on every seed.
    Machine machine = LoadPuzzle("stray_cursor");
    CHECK_FALSE(machine.completed());
    {
        const auto loaded = Bw80Built(machine);
        CHECK(CanPeriod(*loaded, "X1") == 307'692);
        Bw80Run run(*loaded, 1);
        run.runTo(1000_ms);
        CHECK(Bw80DisplayRows(*loaded)[kBw80Rows - 1] == "k");
        for (int i = 0; i < 50; ++i) {
            run.runTo(run.sim.now() + 20_ms);
            CHECK_FALSE(run.tv->tv().isLocked());
        }
    }
    REQUIRE(machine.swap("IC18"));  // the divider: Troubleshooting's step 1
    CHECK_FALSE(machine.completed());
    REQUIRE(machine.swap("X1"));
    CHECK(machine.completed());
    CheckKScreen(machine);
}
