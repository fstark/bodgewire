// The BW-80's netlist (the Sinclair ZX80, P5v): docs/p5v-bw80-video/BW80_VIDEO_DESIGN.md §7.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "bw80_screen.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "netlist/rom_directory.hpp"
#include "parts/key_matrix.hpp"
#include "parts/rom_contents.hpp"
#include "parts/zx80_keys.hpp"
#include "sim/simulator.hpp"
#include "tv_page.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

constexpr int kRows = 24, kColumns = 32;

std::string NetlistText() {
    std::ifstream in(std::filesystem::path(BW_NETLIST_DIR) / "bw80.bwn");
    REQUIRE(in);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The ZX80's character set, $0E00-$0FFF of its ROM.
std::vector<std::uint8_t> Glyphs() {
    const auto rom =
        ReadRomContents(std::filesystem::path(BW_ROOT_DIR) / "roms/zx80_ic2.bin", 4096, 8);
    REQUIRE(rom.has_value());
    return {rom->begin() + 0xE00, rom->end()};
}

// A --roms directory with the test ROM in IC2.
std::filesystem::path TestRomDirectory() {
    const auto dir = std::filesystem::temp_directory_path() / "bw80_test_roms";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::filesystem::create_symlink(std::filesystem::path(BW_ROOT_DIR) / "roms/bw80_test.rom",
                                    dir / "IC2.rom");
    return dir;
}

// The netlist `text` built without a Board (the text variants), with the test ROM in IC2
// through a --roms directory when `testRom`.
std::unique_ptr<LoadedCircuit> Build(const std::string& text, bool testRom) {
    auto netlist = ParseNetlist(text, "bw80.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, BW_NETLIST_DIR);
    if (testRom) {
        REQUIRE(ApplyRomDirectory(*netlist, TestRomDirectory()).has_value());
    }
    auto result = BuildCircuit(*netlist);
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Machine built from its Board (P5x), as ball_test's BoardCircuit; the test ROM through
// --roms when `testRom`.
std::unique_ptr<LoadedCircuit> BoardCircuit(bool testRom) {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "bw80.bwn",
                                      testRom ? std::optional(TestRomDirectory()) : std::nullopt);
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(machine.has_value());
    auto built = machine->build();
    if (!built) {
        for (const Diagnostic& d : built.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(built.has_value());
    return std::move(*built);
}

struct Frames {
    std::vector<TvFrame> frames;
    std::vector<TvFrameStats> stats;
};

// Runs until `count` locked frames or `cap`.
Frames LockedFrames(LoadedCircuit& loaded, std::uint64_t seed, std::size_t count, Time cap) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, loaded.nets.at("VIDEO"), loaded.nets.at("SYNC"));
    Frames result;
    std::uint64_t completed = 0;
    while (sim.now() < cap && result.frames.size() < count) {
        sim.runFor(1_ms);
        tv.update();
        sim.trimRecordings();
        if (tv.tv().completedFrames() != completed) {
            completed = tv.tv().completedFrames();
            if (tv.tv().isLocked()) {
                result.frames.push_back(tv.tv().completedFrame());
                result.stats.push_back(tv.tv().completedStats());
            }
        }
    }
    INFO("simulated " << sim.now() / 1'000'000'000 << " ms");
    return result;
}

// The test ROM's page: cell c (row by row) holds c & $3F, inverse when c & $40.
PageDots TestRomPage() {
    const auto glyphs = Glyphs();
    return ExpectedPage(glyphs, kColumns, kRows, [](int row, int column) {
        const unsigned c = static_cast<unsigned>(row * kColumns + column);
        return (c & 0x3F) | ((c & 0x40) << 1);
    });
}

void CheckSynced(const Frames& f) {
    for (std::size_t i = 1; i < f.frames.size(); ++i) {
        INFO("frame " << i << ": " << f.stats[i].lines << " lines, " << f.stats[i].syncedLines
                      << " synced");
        CHECK(f.stats[i].verticalSynced);
        CHECK(f.stats[i].lines >= 300);
        CHECK(f.stats[i].lines <= 320);
        CHECK(f.stats[i].syncedLines * 10 >= f.stats[i].lines * 9);
    }
}

// A run with the TV plugged in: `runTo` advances it (a ms a slice), `frames` the locked frames
// completed since the last call.
struct Run {
    Simulator sim;
    std::unique_ptr<TvInput> tv;  // plugged in after power-on, as LockedFrames does
    std::uint64_t completed = 0;

    Run(LoadedCircuit& loaded, std::uint64_t seed) : sim(loaded.circuit) {
        sim.powerOn(seed);
        tv = std::make_unique<TvInput>(sim, loaded.nets.at("VIDEO"), loaded.nets.at("SYNC"));
    }
    void runTo(Time t) {
        while (sim.now() < t) {
            sim.runFor(1_ms);
            tv->update();
            sim.trimRecordings();
        }
    }
    // Runs until `count` locked frames completed, or `cap`.
    Frames lockedFrames(std::size_t count, Time cap) {
        Frames result;
        completed = tv->tv().completedFrames();
        while (sim.now() < cap && result.frames.size() < count) {
            runTo(sim.now() + 1_ms);
            if (tv->tv().completedFrames() != completed) {
                completed = tv->tv().completedFrames();
                if (tv->tv().isLocked()) {
                    result.frames.push_back(tv->tv().completedFrame());
                    result.stats.push_back(tv->tv().completedStats());
                }
            }
        }
        return result;
    }
};

// Types `text` on J3 at `at` and runs through the presses (each held 60 ms and released 120 ms)
// plus `settle`.
void Type(Run& run, LoadedCircuit& loaded, std::u32string_view text, Time at, Time settle) {
    run.runTo(at);
    KeyMatrix* keyboard = KeyMatrixOf(loaded, "J3");
    REQUIRE(keyboard != nullptr);
    const std::size_t presses = Zx80Presses(text).size();
    REQUIRE(keyboard->type(text));
    run.sim.wake(keyboard->component().id());
    run.runTo(run.sim.now() + static_cast<Time>(presses) * 180_ms + settle);
}

constexpr Time kTestRomCap = 400_ms;

}  // namespace

TEST_CASE("bw80: netlist loads without warnings", "[machines]") {
    const auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "bw80.bwn");
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
    const auto loaded = BoardCircuit(true);
    const Frames f = LockedFrames(*loaded, seed, 3, kTestRomCap);
    REQUIRE(f.frames.size() == 3);
    CheckSynced(f);
    const PageDots page = TestRomPage();
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
    std::string text = NetlistText();
    const std::string fast = "part IC4  2114 access=300ns";
    REQUIRE(text.find(fast) != std::string::npos);
    text.replace(text.find(fast), fast.size(), "part IC4  2114");
    const auto loaded = Build(text, true);
    const Frames f = LockedFrames(*loaded, 1, 3, kTestRomCap);
    if (f.frames.size() == 3) {
        CHECK_FALSE(FindPage(f.frames[2], TestRomPage()).has_value());
    }
}

TEST_CASE("bw80: the ZX80 ROM shows its K cursor", "[machines][slow]") {
#ifndef NDEBUG
    SKIP("hundreds of ms simulated: runs in the release build");
#endif
    // Sinclair's ROM after power-on: an empty program, the edit line's inverse K cursor at the
    // bottom left, the rest paper.
    const auto loaded = BoardCircuit(false);
    const Frames f = LockedFrames(*loaded, 1, 3, 1000_ms);
    REQUIRE(f.frames.size() == 3);
    CheckSynced(f);
    const auto glyphs = Glyphs();
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
    const int top = std::max(at->second - (kRows - 1) * 8, 0);
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
    auto loaded = BoardCircuit(false);
    Run run(*loaded, 1);
    Type(run, *loaded, U"PRINT 2+2\n", 400_ms, 300_ms);
    const auto rows = Bw80DisplayRows(*loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
    }
    REQUIRE(rows.size() == kBw80Rows);
    CHECK(rows[0] == "4");
    CHECK(rows[kBw80Rows - 1] == "0/-2");
    const Frames f = run.lockedFrames(2, run.sim.now() + 600_ms);
    REQUIRE(f.frames.size() == 2);
    CheckSynced(f);
    const PageDots page = Bw80ExpectedPage(*loaded, Glyphs());
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
    auto loaded = BoardCircuit(false);
    Run run(*loaded, 1);
    Type(run, *loaded, U"10 LET A=6\n20 PRINT A*7\nRUN\n", 400_ms, 400_ms);
    const auto rows = Bw80DisplayRows(*loaded);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        UNSCOPED_INFO("row " << r << ": '" << rows[r] << "'");
    }
    REQUIRE(rows.size() == kBw80Rows);
    CHECK(rows[0] == "42");
    CHECK(rows[kBw80Rows - 1] == "0/20");
}
