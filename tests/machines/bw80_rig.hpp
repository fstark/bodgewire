#pragma once

// Machine 140 BW-80 under test (machines/140-bw80/docs/BW80_DESIGN.md): the Machine built from
// its Board (or a netlist text), run with its TV on J2, typed on through J3, its picture compared
// with the page its display file predicts. Shared by the Machine's fault-free cases and its
// Puzzles (P5y). The TV's Nets are found by connector pin: on a damaged Board Nets are named
// from the Copper.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"
#include "tv_page.hpp"

namespace bw {

std::string Bw80NetlistText();  // machines/140-bw80/bw80.bwn

// The ZX80's character set, $0E00-$0FFF of its ROM: 64 glyphs of 8 rows.
std::vector<std::uint8_t> Bw80Glyphs();

// A --roms directory with the test ROM (machines/140-bw80/roms/bw80_test.rom) in IC2.
std::filesystem::path Bw80TestRomDirectory();

// The netlist `text` built without a Board (the text variants), with the test ROM in IC2
// through a --roms directory when `testRom`.
std::unique_ptr<LoadedCircuit> Bw80Build(const std::string& text, bool testRom);

// The Machine built from its Board (P5x); the test ROM through --roms when `testRom`.
std::unique_ptr<LoadedCircuit> Bw80BoardCircuit(bool testRom);

// A Puzzle's or repaired Machine's Circuit (REQUIREd).
std::unique_ptr<LoadedCircuit> Bw80Built(const Machine& machine);

struct Bw80Frames {
    std::vector<TvFrame> frames;
    std::vector<TvFrameStats> stats;
};

// Runs from power-on with `seed` until `count` locked frames or `cap`.
Bw80Frames Bw80LockedFrames(LoadedCircuit& loaded, std::uint64_t seed, std::size_t count, Time cap);

// Every frame after the first: vertically synced, 300-320 lines, 90 % of them synced.
void Bw80CheckSynced(const Bw80Frames& f);

// The test ROM's page: cell c (row by row) holds c & $3F, inverse when c & $40.
PageDots Bw80TestRomPage();

// A run with the TV plugged in: `runTo` advances it (a ms a slice), `lockedFrames` the locked
// frames completed since the last call.
struct Bw80Run {
    Simulator sim;
    std::unique_ptr<TvInput> tv;  // plugged in after power-on
    std::uint64_t completed = 0;

    Bw80Run(LoadedCircuit& loaded, std::uint64_t seed);
    void runTo(Time t);
    Bw80Frames lockedFrames(std::size_t count, Time cap);  // until `count` or `cap`
};

// Types `text` on J3 at `at` and runs through the presses (each held 60 ms and released 120 ms)
// plus `settle`.
void Bw80Type(Bw80Run& run, LoadedCircuit& loaded, std::u32string_view text, Time at, Time settle);

// A character set seen through a faulty chain, for the page a Puzzle's picture should show:
// glyph c drawn as glyph `code(c)` (a latch bit wrong), or each glyph's rows taken from `rows`
// (a line counter bit wrong: {0, 0, 2, 2, 4, 4, 6, 6}).
std::vector<std::uint8_t> Bw80GlyphsMapped(std::span<const std::uint8_t> glyphs,
                                           const std::function<unsigned(unsigned)>& code);
std::vector<std::uint8_t> Bw80GlyphsRows(std::span<const std::uint8_t> glyphs,
                                         const std::array<int, 8>& rows);

inline constexpr int kBw80TvRows = 24, kBw80TvColumns = 32;

}  // namespace bw
