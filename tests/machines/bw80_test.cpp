// The BW-80's netlist (the Sinclair ZX80, P5v): docs/p5v-bw80-video/BW80_VIDEO_DESIGN.md §7;
// the Machine (P5x): machines/140-bw80/docs/BW80_DESIGN.md. Helpers in bw80_rig.{hpp,cpp}.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../paths.hpp"
#include "bw80_rig.hpp"
#include "bw80_screen.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "tv_page.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

constexpr Time kTestRomCap = 400_ms;

}  // namespace

TEST_CASE("bw80: netlist loads without warnings", "[machines]") {
    const auto loaded = LoadNetlistFile(test::NetlistPath("bw80"));
    if (!loaded) {
        for (const Diagnostic& d : loaded.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(loaded.has_value());
    for (const Diagnostic& d : (*loaded)->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK((*loaded)->warnings.empty());
}

TEST_CASE("bw80: the test ROM shows every character", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("hundreds of ms simulated: runs in the release build");
#endif
    const std::uint64_t seed = GENERATE(1, 2);
    INFO("seed " << seed);
    const auto loaded = Bw80BoardCircuit(true);
    const Bw80Frames f = Bw80LockedFrames(*loaded, seed, 3, kTestRomCap);
    REQUIRE(f.frames.size() == 3);
    Bw80CheckSynced(f);
    const PageDots page = Bw80TestRomPage();
    const auto at = FindPage(f.frames[1], page);
    REQUIRE(at.has_value());
    INFO("left " << at->first << ", top " << at->second);
    CHECK(Matches(f.frames[2], page, at->first, at->second));
}

TEST_CASE("bw80: a 450 ns RAM misses the NOP", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("hundreds of ms simulated: runs in the release build");
#endif
    // IC4 at the slowest 2114 grade: its data comes too late for the NOP gates, so the CPU runs
    // the display file's codes as instructions. No page, whether or not the TV locks.
    std::string text = Bw80NetlistText();
    const std::string fast = "part IC4  2114 access=300ns";
    REQUIRE(text.find(fast) != std::string::npos);
    text.replace(text.find(fast), fast.size(), "part IC4  2114");
    const auto loaded = Bw80Build(text, true);
    const Bw80Frames f = Bw80LockedFrames(*loaded, 1, 3, kTestRomCap);
    if (f.frames.size() == 3) {
        CHECK_FALSE(FindPage(f.frames[2], Bw80TestRomPage()).has_value());
    }
}

TEST_CASE("bw80: the ZX80 ROM shows its K cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("hundreds of ms simulated: runs in the release build");
#endif
    // Sinclair's ROM after power-on: an empty program, the edit line's inverse K cursor at the
    // bottom left, the rest paper.
    const auto loaded = Bw80BoardCircuit(false);
    const Bw80Frames f = Bw80LockedFrames(*loaded, 1, 3, 1000_ms);
    REQUIRE(f.frames.size() == 3);
    Bw80CheckSynced(f);
    const auto glyphs = Bw80Glyphs();
    const PageDots cursor = ExpectedPage(glyphs, 1, 1, [](int, int) { return 0xB0u; });
    const auto at = FindPage(f.frames[2], cursor);
    REQUIRE(at.has_value());
    INFO("left " << at->first << ", top " << at->second);
    // The page: the 24 rows ending with the K's (the edit line is the last row: LISTING ED-COPY),
    // 32 cells from its left edge. No ink there but the K's. The margins are not looked at: the
    // TV draws the sync periods too, unblanked (Design §7).
    const double columnsPerDot = 1e12 / 6.5e6 / static_cast<double>(Tv::kColumnTime);
    const int left = static_cast<int>(std::floor(at->first));
    const int right =
        std::min(static_cast<int>(std::ceil(at->first + 256 * columnsPerDot)), kTvColumns - 1);
    const int cellRight = static_cast<int>(std::ceil(at->first + 8 * columnsPerDot));
    const int top = std::max(at->second - (kBw80TvRows - 1) * 8, 0);
    int stray = 0;
    for (int row = top; row < at->second + 8; ++row) {
        int inRow = 0;
        for (int column = left; column <= right; ++column) {
            const bool inCell = row >= at->second && column <= cellRight;
            inRow += !inCell && f.frames[2].at(row, column) < 0.5f ? 1 : 0;
        }
        if (inRow > 0) {
            UNSCOPED_INFO("row " << row << ": " << inRow << " stray ink points");
        }
        stray += inRow;
    }
    CHECK(stray == 0);
}

TEST_CASE("bw80: PRINT 2+2 typed through J3", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("seconds simulated: runs in the release build");
#endif
    // The ROM clears its RAM and shows the K cursor by about 200 ms; then the line typed: O for
    // PRINT (K mode), 2, SHIFT+K, 2, NEW LINE. The answer at the top of the screen, the report
    // at the bottom (code 0, no error; the line -2, the edit line: a direct command), in the
    // display file and on the TV, which locks again about 100 ms after the picture returns.
    auto loaded = Bw80BoardCircuit(false);
    Bw80Run run(*loaded, 1);
    Bw80Type(run, *loaded, U"PRINT 2+2\n", 400_ms, 300_ms);
    const auto rows = Bw80DisplayRows(*loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
    }
    REQUIRE(rows.size() == kBw80Rows);
    CHECK(rows[0] == "4");
    CHECK(rows[kBw80Rows - 1] == "0/-2");
    const Bw80Frames f = run.lockedFrames(2, run.sim.now() + 600_ms);
    REQUIRE(f.frames.size() == 2);
    Bw80CheckSynced(f);
    const PageDots page = Bw80ExpectedPage(*loaded, Bw80Glyphs());
    const auto at = FindPage(f.frames[1], page);
    REQUIRE(at.has_value());
    INFO("left " << at->first << ", top " << at->second);
}

TEST_CASE("bw80: a program is entered and run", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("seconds simulated: runs in the release build");
#endif
    // Two lines through the tokenizer (LET, PRINT, * as SHIFT+P), then RUN: 42 at the top, the
    // report 0/20 (no error, after line 20) at the bottom.
    auto loaded = Bw80BoardCircuit(false);
    Bw80Run run(*loaded, 1);
    Bw80Type(run, *loaded, U"10 LET A=6\n20 PRINT A*7\nRUN\n", 400_ms, 400_ms);
    const auto rows = Bw80DisplayRows(*loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
    }
    REQUIRE(rows.size() == kBw80Rows);
    CHECK(rows[0] == "42");
    CHECK(rows[kBw80Rows - 1] == "0/20");
}
