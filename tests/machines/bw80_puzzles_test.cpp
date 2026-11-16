// Machine 140 BW-80 Puzzles (docs/machines/140-bw80/BW80_DESIGN.md, Puzzles): each from
// power-on, faulty, read as the player reads it (the TV against the page the display file
// predicts, the display file in the two 2114s, the probe's clue), then repaired. Release-only
// but the first two: debug runs the Machine at about 0.004x.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "bw80_rig.hpp"
#include "bw80_screen.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/diagnostic.hpp"
#include "tv_page.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// The Puzzles, in BW80.md's order.
constexpr std::array kPuzzles{"q_everywhere",  "doubled_lines",     "bottom_halves",
                              "wrong_letters", "quotes_for_spaces", "plain_cursor",
                              "blank_page"};

using CodeMap = std::function<unsigned(unsigned)>;
const CodeMap kSameCode = [](unsigned code) { return code; };

// The page the display file predicts through `glyphs`, each code through `map` (the inverse
// bit included). The cells past a row's end are blank whatever the chain does to a space (no
// code is loaded there) unless `fill` names a code (Q everywhere: the HALT's glyph).
PageDots PageOf(const LoadedCircuit& loaded, std::span<const std::uint8_t> glyphs,
                const CodeMap& map = kSameCode, std::optional<unsigned> fill = std::nullopt) {
    const auto rows = Bw80DisplayCodes(loaded);
    const auto has = [&](int row, int column) {
        const auto r = static_cast<std::size_t>(row), c = static_cast<std::size_t>(column);
        return r < rows.size() && c < rows[r].size();
    };
    PageDots page = ExpectedPage(glyphs, kBw80TvColumns, kBw80TvRows, [&](int row, int column) {
        const auto r = static_cast<std::size_t>(row), c = static_cast<std::size_t>(column);
        return map(has(row, column) ? rows[r][c] : fill.value_or(0));
    });
    if (!fill) {
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
    }
    return page;
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

// The picture is `page` and not the fault-free one.
void CheckPicture(const Typed& t, const PageDots& page) {
    CHECK(FindPage(t.frame, page).has_value());
    CHECK_FALSE(FindPage(t.frame, PageOf(*t.loaded, Bw80Glyphs())).has_value());
}

}  // namespace

TEST_CASE("bw80: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
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
    // The HALT ending each row loads the glyph of $36, Q, which repeats to the row's end.
    Machine machine = LoadPuzzle("q_everywhere");
    {
        const Typed t = TypeAndLook(machine);
        CheckPicture(t, PageOf(*t.loaded, Bw80Glyphs(), kSameCode, 0x36));
    }
    REQUIRE(machine.swap("IC16"));
    CheckKScreen(machine);
}

TEST_CASE("bw80: doubled_lines", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("doubled_lines");
    {
        const Typed t = TypeAndLook(machine);
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsRows(Bw80Glyphs(), {0, 0, 2, 2, 4, 4, 6, 6})));
    }
    REQUIRE(machine.swap("IC21"));
    CheckKScreen(machine);
}

TEST_CASE("bw80: bottom_halves", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    Machine machine = LoadPuzzle("bottom_halves");
    {
        const Typed t = TypeAndLook(machine);
        CHECK(*PinNet(*t.loaded, "IC8", "4A") != *PinNet(*t.loaded, "IC21", "QD"));
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsRows(Bw80Glyphs(), {4, 5, 6, 7, 4, 5, 6, 7})));
    }
    REQUIRE(machine.mend(BottomHalvesCrack().layer, BottomHalvesCrack().at));
    CheckKScreen(machine);
}

TEST_CASE("bw80: wrong_letters", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // CH1 and CH2 bridged: where they differ both read low (TTL contention), so the glyph is
    // the code's with bits 1 and 2 cleared unless both are set.
    Machine machine = LoadPuzzle("wrong_letters");
    {
        const Typed t = TypeAndLook(machine);
        CHECK(Bw80DisplayRows(*t.loaded)[0].find("PRINT \"HELLO 0123\"") != std::string::npos);
        const auto bridged = [](unsigned code) {
            return (code & 0x06) == 0x06 ? code : (code & ~0x06u);
        };
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsMapped(Bw80Glyphs(), bridged)));
    }
    REQUIRE(machine.removeBridge({"IC5", "15"}, {"IC5", "16"}));
    CheckKScreen(machine);
}

TEST_CASE("bw80: quotes_for_spaces", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // CH0's dry Joint: IC8 reads it high, so every even code is drawn as the next one.
    Machine machine = LoadPuzzle("quotes_for_spaces");
    {
        const Typed t = TypeAndLook(machine);
        CHECK(*PinNet(*t.loaded, "IC5", "3Q") != *PinNet(*t.loaded, "IC8", "1A"));
        CheckPicture(t, PageOf(*t.loaded, Bw80GlyphsMapped(Bw80Glyphs(), [](unsigned code) {
                         return code | 1;
                     })));
    }
    REQUIRE(machine.resolder({"IC5", "6"}));
    CheckKScreen(machine);
}

TEST_CASE("bw80: plain_cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 1 s simulated: runs in the release build");
#endif
    // The inverse latch stuck: the cursor's $B0 drawn as a plain K.
    Machine machine = LoadPuzzle("plain_cursor");
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
    CheckKScreen(machine);
}

TEST_CASE("bw80: blank_page", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("about 6 s simulated: runs in the release build");
#endif
    // The shift register never shifts: no dots, only the inverse latch's black square at the
    // cursor; the display file holds the listing all the same.
    Machine machine = LoadPuzzle("blank_page");
    {
        const Typed t = TypeAndLook(machine);
        CHECK(Bw80DisplayRows(*t.loaded)[0].find("PRINT \"HELLO 0123\"") != std::string::npos);
        const std::vector<std::uint8_t> blank(Bw80Glyphs().size(), 0);
        CheckPicture(t, PageOf(*t.loaded, blank));
        CHECK(InkCount(t.frame) > 0);
    }
    REQUIRE(machine.swap("IC9"));
    CheckKScreen(machine);
}
