#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/builder.hpp"
#include "parts/rom_contents.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// One dot is 4 TV columns (0.5 µs / 125 ns), one line one row.
constexpr int kDot = 4;
constexpr int kColumns = 16, kRows = 16;
constexpr int kCellDots = 6, kCellLines = 16;
constexpr int kPageDots = kColumns * kCellDots;  // 96
constexpr int kPageLines = kRows * kCellLines;   // 256

bool Lit(const TvFrame& frame, int row, int column) { return frame.at(row, column) >= 0.5f; }

// How a Puzzle's Fault changes the lookups (MESSAGE.md, Puzzles); the default is the real page.
struct PageFault {
    unsigned columnOr = 0;     // Stammer: the message ROM's A0 reads high
    unsigned codeOr = 0;       // Gibberish: the character ROM's A9 reads high
    unsigned glyphLines = 7;   // Half letters: R3 held low
    unsigned dotsMask = 0x1F;  // Thin letters: F1 held low
    bool blocks = false;       // Block letters: F5 shown on all six dots
    int fontWord = -1;         // a character ROM word with stuck cells, ANDed with fontAnd
    unsigned fontAnd = 0x1F;
};

// The page as dots, from the two ROM files: [line][dot].
std::vector<std::vector<bool>> ExpectedPage(const PageFault& fault = {}) {
    const std::filesystem::path roms = test::MachineDir("message") / "roms";
    const auto message = ReadRomContents(roms / "message.rom", 2048, 8);
    const auto font = ReadRomContents(roms / "2513.rom", 512, 5);
    REQUIRE(message.has_value());
    REQUIRE(font.has_value());
    std::vector<std::vector<bool>> page(kPageLines, std::vector<bool>(kPageDots, false));
    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            const unsigned address =
                static_cast<unsigned>(row * kColumns + column) | fault.columnOr;
            const unsigned code = ((*message)[address] & 0x3F) | fault.codeOr;
            for (int line = 0; line < kCellLines; ++line) {
                const unsigned glyphLine = static_cast<unsigned>(line / 2) & fault.glyphLines;
                const unsigned word = code * 8 + glyphLine;
                unsigned dots = (*font)[word] & fault.dotsMask;
                if (static_cast<int>(word) == fault.fontWord) {
                    dots &= fault.fontAnd;
                }
                auto& out = page[static_cast<std::size_t>(row * kCellLines + line)];
                for (int x = 0; x < (fault.blocks ? kCellDots : 5); ++x) {
                    const int bit = fault.blocks ? 4 : 4 - x;
                    out[static_cast<std::size_t>(column * kCellDots + x)] = (dots >> bit) & 1;
                }
            }
        }
    }
    return page;
}

struct Offset {
    int left, top;
};

bool Matches(const TvFrame& frame, const std::vector<std::vector<bool>>& page, int left, int top) {
    for (int y = 0; y < kPageLines; ++y) {
        for (int x = 0; x < kPageDots; ++x) {
            const int column = left + kDot * x + kDot / 2;
            if (top + y >= kTvRows || column >= kTvColumns) {
                return false;
            }
            if (Lit(frame, top + y, column) !=
                page[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) {
                return false;
            }
        }
    }
    return true;
}

// Where the page is on the frame: the middle of the one run of matching columns (a dot is 4
// columns wide); nullopt unless exactly one run of at most 4 matches.
std::optional<Offset> FindPage(const TvFrame& frame, const std::vector<std::vector<bool>>& page) {
    std::vector<Offset> matches;
    for (int top = 0; top < 64; ++top) {
        for (int left = 0; left < 128; ++left) {
            if (Matches(frame, page, left, top)) {
                matches.push_back({left, top});
            }
        }
    }
    if (matches.empty() || matches.size() > kDot) {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < matches.size(); ++i) {
        if (matches[i].top != matches[0].top || matches[i].left != matches[i - 1].left + 1) {
            return std::nullopt;
        }
    }
    return matches[matches.size() / 2];
}

// Lit cells outside the text rectangle at `offset`.
int LitOutside(const TvFrame& frame, Offset offset) {
    int lit = 0;
    const int left = offset.left - kDot / 2;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            const bool inside = row >= offset.top && row < offset.top + kPageLines &&
                                column >= left && column < left + kDot * kPageDots;
            lit += !inside && Lit(frame, row, column) ? 1 : 0;
        }
    }
    return lit;
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(test::NetlistPath("message"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(test::NetlistPath("message"));
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// Frames completed from power-on until `count` locked frames were seen (at most 1 s).
std::vector<TvFrame> LockedFrames(LoadedCircuit& loaded, std::uint64_t seed, int count) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, *ConnectorNet(loaded, "VIDEO", "VIDEO"),
               *ConnectorNet(loaded, "VIDEO", "SYNC_N"));
    std::vector<TvFrame> frames;
    std::uint64_t completed = 0;
    while (sim.now() < 1_s && static_cast<int>(frames.size()) < count) {
        sim.runFor(1_ms);
        tv.update();
        sim.trimRecordings();
        if (tv.tv().completedFrames() != completed) {
            completed = tv.tv().completedFrames();
            if (tv.tv().isLocked()) {
                frames.push_back(tv.tv().completedFrame());
            }
        }
    }
    return frames;
}

}  // namespace

TEST_CASE("message: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("message: the TV shows the page", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    const auto page = ExpectedPage();
    auto loaded = BoardCircuit();
    // The first locked frame may have started before the counters fell into step.
    const auto frames = LockedFrames(*loaded, seed, 3);
    REQUIRE(frames.size() == 3);
    const auto offset = FindPage(frames[1], page);
    REQUIRE(offset.has_value());
    INFO("left " << offset->left << ", top " << offset->top);
    CHECK(offset->left >= 41);
    CHECK(offset->left <= 47);
    CHECK(offset->top <= 8);
    CHECK(LitOutside(frames[1], *offset) == 0);
    CHECK(frames[2].cells == frames[1].cells);
}

// Puzzles (MESSAGE.md, Puzzles): each from power-on (seed 1), faulty then repaired.

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

std::unique_ptr<LoadedCircuit> Build(const Machine& machine) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The second of three locked frames, where `page` is found (nullopt if it is not, alone).
std::optional<Offset> ShowsPage(LoadedCircuit& loaded, const std::vector<std::vector<bool>>& page,
                                TvFrame* shown = nullptr) {
    const auto frames = LockedFrames(loaded, 1, 3);
    REQUIRE(frames.size() == 3);
    if (shown != nullptr) {
        *shown = frames[1];
    }
    return FindPage(frames[1], page);
}

// The real page is back, with nothing outside it.
void CheckRepaired(const Machine& machine) {
    auto loaded = Build(machine);
    TvFrame frame;
    const auto offset = ShowsPage(*loaded, ExpectedPage(), &frame);
    REQUIRE(offset.has_value());
    CHECK(LitOutside(frame, *offset) == 0);
}

// The faulty page at the real page's place, with nothing outside it.
void CheckFaultyPage(const Machine& machine, const PageFault& fault) {
    auto loaded = Build(machine);
    TvFrame frame;
    CHECK_FALSE(ShowsPage(*loaded, ExpectedPage()).has_value());
    const auto offset = ShowsPage(*loaded, ExpectedPage(fault), &frame);
    REQUIRE(offset.has_value());
    CHECK(offset->left >= 41);
    CHECK(offset->left <= 47);
    CHECK(offset->top <= 8);
    CHECK(LitOutside(frame, *offset) == 0);
}

}  // namespace

TEST_CASE("message: Puzzles have a title and a brief", "[machines]") {
    for (const char* name :
         {"half_letters", "echo", "gibberish", "stammer", "thin_letters", "block_letters"}) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "message");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("message: half_letters", "[machines][slow]") {
    Machine machine = LoadPuzzle("half_letters");
    CheckFaultyPage(machine, {.glyphLines = 3});
    REQUIRE(machine.swap("U13"));
    CheckRepaired(machine);
}

TEST_CASE("message: echo", "[machines][slow]") {
    Machine machine = LoadPuzzle("echo");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U12", "1B") == *PinNet(*loaded, "U8", "QA"));  // V8 unchanged
        TvFrame frame;
        const auto page = ExpectedPage();
        const auto offset = ShowsPage(*loaded, page, &frame);
        REQUIRE(offset.has_value());  // the page itself is whole
        CHECK(LitOutside(frame, *offset) > 0);
        // Lines 288–303 draw text row 2 again, the poem's first line.
        const std::vector<std::vector<bool>> first(page.begin() + 2 * kCellLines,
                                                   page.begin() + 3 * kCellLines);
        bool echoed = true;
        for (int y = 0; y < kCellLines; ++y) {
            for (int x = 0; x < kPageDots; ++x) {
                const int column = offset->left + kDot * x + kDot / 2;
                echoed =
                    echoed && Lit(frame, offset->top + 288 + y, column) ==
                                  first[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            }
        }
        CHECK(echoed);
    }
    REQUIRE(machine.swap("U12"));
    CheckRepaired(machine);
}

TEST_CASE("message: gibberish", "[machines][slow]") {
    Machine machine = LoadPuzzle("gibberish");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U15", "A9") != *PinNet(*loaded, "U14", "O5"));
    }
    CheckFaultyPage(machine, {.codeOr = 0x20});
    REQUIRE(machine.mend(GibberishCrack().layer, GibberishCrack().at));
    CheckRepaired(machine);
}

TEST_CASE("message: stammer", "[machines][slow]") {
    Machine machine = LoadPuzzle("stammer");
    {
        auto loaded = Build(machine);
        const Point pin8 = *machine.padPosition("U14", "8");
        CHECK(machine.probeNet(*loaded, Layer::Back, pin8) == PinNet(*loaded, "U2", "QA"));
        CHECK(*PinNet(*loaded, "U14", "A0") != *PinNet(*loaded, "U2", "QA"));  // the leg
    }
    CheckFaultyPage(machine, {.columnOr = 1});
    REQUIRE(machine.resolder({"U14", "8"}));
    CheckRepaired(machine);
}

TEST_CASE("message: thin_letters", "[machines][slow]") {
    Machine machine = LoadPuzzle("thin_letters");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U16", "D") == *PinNet(*loaded, "U16", "C"));  // F1 on GND
        CHECK(*PinNet(*loaded, "U15", "O1") == *PinNet(*loaded, "U16", "C"));
    }
    CheckFaultyPage(machine, {.dotsMask = 0x1E});
    REQUIRE(machine.swap("U15"));  // the tempting wrong repair
    CheckFaultyPage(machine, {.dotsMask = 0x1E});
    REQUIRE(machine.removeBridge({"U16", "13"}, {"U16", "14"}));
    CheckRepaired(machine);
}

TEST_CASE("message: block_letters", "[machines][slow]") {
    Machine machine = LoadPuzzle("block_letters");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U16", "CLK") != *PinNet(*loaded, "U5", "3Y"));
    }
    CheckFaultyPage(machine, {.blocks = true});
    REQUIRE(machine.mend(BlockLettersCrack().layer, BlockLettersCrack().at));
    CheckRepaired(machine);
}

// Memory Faults inside the character ROM (P5g), from inline Puzzles: no Puzzle file yet.

TEST_CASE("message: a stuck column in the character ROM", "[machines][slow]") {
    Machine machine = InlinePuzzle("machine message\nfault column U15.O1 0\n");
    CheckFaultyPage(machine, {.dotsMask = 0x1E});  // thin_letters' page, made inside U15
    REQUIRE(machine.swap("U15"));
    CheckRepaired(machine);
}

TEST_CASE("message: a stuck cell in the character ROM", "[machines][slow]") {
    // E (code 5), glyph line 1: word $29 = #####; O3 (bit 2) stuck at 0 takes out the middle dot.
    Machine machine = InlinePuzzle("machine message\nfault cell U15.O3 $29 0\n");
    CheckFaultyPage(machine, {.fontWord = 0x29, .fontAnd = 0x1B});
    REQUIRE(machine.swap("U15"));
    CheckRepaired(machine);
}
