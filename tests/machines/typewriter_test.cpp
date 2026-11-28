// 090 TV typewriter (machines/090-typewriter/docs/TYPEWRITER.md): the shift-register page on the
// TV.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
#include "parts/keyboard.hpp"
#include "parts/rom_contents.hpp"
#include "parts/shift_register_2518.hpp"
#include "parts/shift_register_2524.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"
#include "tv_window.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// One dot is 2 TV columns (0.25 µs / 125 ns), one line one row.
constexpr int kDot = 2;
constexpr int kColumns = 32, kRows = 16, kCells = kColumns * kRows;
constexpr int kCellDots = 6, kCellLines = 16;
constexpr int kPageDots = kColumns * kCellDots;  // 192
constexpr int kPageLines = kRows * kCellLines;   // 256

using Page = std::vector<std::vector<bool>>;  // [line][dot]
using Codes = std::array<unsigned, kCells>;   // 6-bit codes, row × 32 + column

struct Cell {
    int column, row;
    bool operator==(const Cell&) const = default;
};

bool Lit(const TvFrame& frame, int row, int column) { return frame.at(row, column) >= 0.5f; }

const std::vector<std::uint8_t>& Font() {
    static const std::vector<std::uint8_t> font = [] {
        auto read = ReadRomContents(test::RomPath("typewriter", "2513.rom"), 512, 5);
        REQUIRE(read.has_value());
        return *read;
    }();
    return font;
}

// The page as dots from the cells' codes; the `cursor` cell inverted (all 6 dots, 16 lines).
Page ExpectedPage(const Codes& codes, std::optional<Cell> cursor = std::nullopt) {
    Page page(kPageLines, std::vector<bool>(kPageDots, false));
    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            const unsigned code = codes[static_cast<std::size_t>(row * kColumns + column)];
            const bool inverted = cursor == Cell{column, row};
            for (int line = 0; line < kCellLines; ++line) {
                const unsigned dots = Font()[code * 8 + static_cast<unsigned>(line / 2)];
                auto& out = page[static_cast<std::size_t>(row * kCellLines + line)];
                for (int x = 0; x < kCellDots; ++x) {
                    const bool lit = x < 5 && ((dots >> (4 - x)) & 1) != 0;
                    out[static_cast<std::size_t>(column * kCellDots + x)] = lit != inverted;
                }
            }
        }
    }
    return page;
}

struct Offset {
    int left, top;
};

bool Matches(const TvFrame& frame, const Page& page, int left, int top) {
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

// Where the page is on the frame: the middle of the one run of matching columns (a dot is 2
// columns wide); nullopt unless exactly one run of at most 2 matches.
std::optional<Offset> FindPage(const TvFrame& frame, const Page& page) {
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

// Where the page of `codes` is on the frame, with the cursor's cell either inverted or plain (it
// blinks); nullopt unless exactly one of the two is found.
std::optional<Offset> ShownAt(const TvFrame& frame, const Codes& codes, Cell cursor,
                              bool* inverted = nullptr) {
    const auto plain = FindPage(frame, ExpectedPage(codes));
    const auto withCursor = FindPage(frame, ExpectedPage(codes, cursor));
    if (plain.has_value() == withCursor.has_value()) {
        return std::nullopt;
    }
    if (inverted != nullptr) {
        *inverted = withCursor.has_value();
    }
    return plain ? plain : withCursor;
}

// Lit cells outside the text rectangle at `offset`, widened by one dot on the right (a cursor in
// column 31 lights its sixth dot).
int LitOutside(const TvFrame& frame, Offset offset) {
    int lit = 0;
    const int left = offset.left - kDot / 2;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            const bool inside = row >= offset.top && row < offset.top + kPageLines &&
                                column >= left && column < left + kDot * (kPageDots + 1);
            lit += !inside && Lit(frame, row, column) ? 1 : 0;
        }
    }
    return lit;
}

// The Machine built from its Board (machines/090-typewriter/board).
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(test::NetlistPath("typewriter"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(test::NetlistPath("typewriter"));
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Machine (its Board, or a Puzzle's) on a simulator, powered on.
struct Typewriter {
    std::unique_ptr<LoadedCircuit> loaded;
    Simulator sim{loaded->circuit};

    explicit Typewriter(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit = BoardCircuit())
        : loaded(std::move(circuit)) {
        sim.powerOn(seed);
        sim.runFor(1_ms);
    }

    // Clear (SW1) held for 25 ms, more than a frame, then released.
    void clear() {
        Switch* button = SwitchOf(*loaded, "SW1");
        button->setPressed(true);
        sim.wake(button->id());
        sim.runFor(25_ms);
        button->setPressed(false);
        sim.wake(button->id());
        sim.runFor(1_ms);
    }

    // The codes typed on J3, handed over at most 32 at a time (the keys the keyboard keeps
    // waiting); 26 ms a key (a 25 ms strobe and a little) and 20 ms more for the last key's cell
    // to come round.
    void type(const std::vector<std::uint8_t>& keys) {
        Keyboard* keyboard = KeyboardOf(*loaded, "J3");
        for (std::size_t first = 0; first < keys.size(); first += kKeyboardWaiting) {
            const std::size_t count = std::min(kKeyboardWaiting, keys.size() - first);
            for (std::size_t i = first; i < first + count; ++i) {
                REQUIRE(keyboard->type(keys[i]));
            }
            sim.wake(keyboard->id());
            sim.runFor(26_ms * static_cast<Time>(count) + 20_ms);
        }
    }
    void type(std::string_view text) { type(std::vector<std::uint8_t>(text.begin(), text.end())); }

    // The cursor counter (X0-X4, Y0-Y3); nullopt if a bit is not 0 or 1.
    std::optional<Cell> cursor() const {
        auto bits = [&](const char* prefix, int count) -> std::optional<int> {
            int value = 0;
            for (int bit = 0; bit < count; ++bit) {
                const LogicValue v = sim.value(loaded->nets.at(prefix + std::to_string(bit)));
                if (v != Zero && v != One) {
                    return std::nullopt;
                }
                value |= (v == One ? 1 : 0) << bit;
            }
            return value;
        };
        const auto x = bits("X", 5), y = bits("Y", 4);
        if (!x || !y) {
            return std::nullopt;
        }
        return Cell{*x, *y};
    }

    // Runs on into the vertical blanking (V8 is 1).
    void toBlanking() {
        const NetId v8 = loaded->nets.at("V8");
        for (int i = 0; i < 400 && sim.value(v8) != One; ++i) {
            sim.runFor(100_us);
        }
        REQUIRE(sim.value(v8) == One);
    }

    // The page's cells, read in the vertical blanking: after the last top line of a frame the
    // page's output holds cell 0.
    Codes codes() {
        toBlanking();
        std::array<const ShiftRegister2524*, 6> page{};
        for (std::size_t bit = 0; bit < page.size(); ++bit) {
            page[bit] = &static_cast<const ShiftRegister2524&>(
                *loaded->components.at("U" + std::to_string(16 + bit)));
        }
        Codes codes{};
        for (std::size_t i = 0; i < codes.size(); ++i) {
            for (std::size_t bit = 0; bit < page.size(); ++bit) {
                codes[i] |= (page[bit]->bit(i) ? 1u : 0u) << bit;
            }
        }
        return codes;
    }

    // The row buffer's 32 words as every row would show them, read in the vertical blanking: after
    // the last text line the buffer has gone round once, word 0 at its outputs.
    Codes rowBuffer() {
        toBlanking();
        const auto& buffer = static_cast<const ShiftRegister2518&>(*loaded->components.at("U22"));
        Codes codes{};
        for (std::size_t i = 0; i < codes.size(); ++i) {
            codes[i] = buffer.word(i % kColumns);
        }
        return codes;
    }

    // Frames completed from now until `count` locked frames were seen (at most 1 s more).
    std::vector<TvFrame> lockedFrames(int count) {
        TvInput tv(sim, *ConnectorNet(*loaded, "VIDEO", "VIDEO"),
                   *ConnectorNet(*loaded, "VIDEO", "SYNC_N"));
        std::vector<TvFrame> frames;
        std::uint64_t completed = 0;
        const Time end = sim.now() + 1_s;
        while (sim.now() < end && static_cast<int>(frames.size()) < count) {
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
};

}  // namespace

TEST_CASE("typewriter: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("typewriter: the TV shows the page", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1, 2);
    INFO("seed " << seed);
    Typewriter t(seed);
    // The first locked frame may have started before the counters fell into step.
    const auto frames = t.lockedFrames(3);
    REQUIRE(frames.size() == 3);
    const auto cursor = t.cursor();
    REQUIRE(cursor.has_value());
    const Codes codes = t.codes();
    const auto offset = ShownAt(frames[1], codes, *cursor);
    REQUIRE(offset.has_value());
    INFO("left " << offset->left << ", top " << offset->top);
    // The line sync on characters 34-35 and the vertical sync on lines 280-283 (P6n): the text
    // 12 µs after the sync edge (column 106), line 0 on row 32, inside a monitor's window.
    CHECK(offset->left >= 102);
    CHECK(offset->left <= 110);
    CHECK(offset->top >= 30);
    CHECK(offset->top <= 34);
    CHECK(LitOutside(frames[1], *offset) == 0);
    const auto lit = LitBounds(frames[1]);
    REQUIRE(lit.has_value());
    CHECK(InsideWindow(*lit, 312));
    CHECK(ShownAt(frames[2], codes, *cursor).has_value());
}

namespace {

constexpr unsigned kSpace = 0x20;

// The cleared page with `text` from cell `first` on, each character's code & 0x3F.
Codes Typed(std::string_view text, std::size_t first = 0) {
    Codes codes;
    codes.fill(kSpace);
    for (std::size_t i = 0; i < text.size(); ++i) {
        codes[first + i] = static_cast<unsigned char>(text[i]) & 0x3Fu;
    }
    return codes;
}

}  // namespace

TEST_CASE("typewriter: Clear blanks the page and homes the cursor", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    Typewriter t(seed);
    t.clear();
    CHECK(t.codes() == Typed(""));
    CHECK(t.cursor() == Cell{0, 0});
}

TEST_CASE("typewriter: keys land at the cursor", "[machines]") {
    Typewriter t(1);
    t.clear();
    t.type("HELLO, WORLD");
    CHECK(t.codes() == Typed("HELLO, WORLD"));
    CHECK(t.cursor() == Cell{12, 0});
}

TEST_CASE("typewriter: Return starts a line, other control keys do nothing", "[machines]") {
    Typewriter t(2);
    t.clear();
    t.type("AB\rC");
    t.type(std::vector<std::uint8_t>{8, 27, 1});  // Backspace, Esc, CTRL-A
    Codes expected = Typed("AB");
    expected[kColumns] = 'C' & 0x3F;
    CHECK(t.codes() == expected);
    CHECK(t.cursor() == Cell{1, 1});
}

TEST_CASE("typewriter: lines wrap", "[machines][slow]") {
    Typewriter t(3);
    t.clear();
    const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFG";  // 33
    t.type(letters);
    CHECK(t.codes() == Typed(letters));
    CHECK(t.cursor() == Cell{1, 1});
}

TEST_CASE("typewriter: the page wraps", "[machines][slow]") {
    Typewriter t(4);
    t.clear();
    t.type(std::string(16, '\r'));
    CHECK(t.cursor() == Cell{0, 0});
    t.type("Z");
    CHECK(t.codes() == Typed("Z"));
    CHECK(t.cursor() == Cell{1, 0});
}

TEST_CASE("typewriter: the typed page and a blinking cursor", "[machines][slow]") {
    Typewriter t(1);
    t.clear();
    t.type("HELLO\rWORLD");
    REQUIRE(t.cursor() == Cell{5, 1});
    Codes expected = Typed("HELLO");
    for (std::size_t i = 0; i < 5; ++i) {
        expected[kColumns + i] = static_cast<unsigned char>("WORLD"[i]) & 0x3Fu;
    }
    REQUIRE(t.codes() == expected);
    const auto frames = t.lockedFrames(20);
    REQUIRE(frames.size() == 20);
    int on = 0, off = 0;
    for (std::size_t i = 1; i < frames.size(); ++i) {
        INFO("frame " << i);
        bool inverted = false;
        const auto offset = ShownAt(frames[i], expected, Cell{5, 1}, &inverted);
        REQUIRE(offset.has_value());
        CHECK(LitOutside(frames[i], *offset) == 0);
        (inverted ? on : off) += 1;
    }
    CHECK(on > 0);
    CHECK(off > 0);
}

// Puzzles (TYPEWRITER.md, Puzzles): each from power-on (seed 1), Clear pressed, keys typed; faulty,
// then repaired.

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

// Clear, then `text` typed.
Typewriter Typing(const Machine& machine, std::string_view text) {
    Typewriter t(1, Build(machine));
    t.clear();
    t.type(text);
    return t;
}

// The TV shows `codes` (with the blinking cursor) and nothing outside it. The tests keep the cursor
// off text row 0 from before P6n, when the frame's first line, drawn a few characters left, was
// text row 0's; it is a blank line in the back porch now.
void CheckShown(Typewriter& t, const Codes& codes) {
    const auto cursor = t.cursor();
    REQUIRE(cursor.has_value());
    const auto frames = t.lockedFrames(2);
    REQUIRE(frames.size() == 2);
    const auto offset = ShownAt(frames[1], codes, *cursor);
    REQUIRE(offset.has_value());
    CHECK(LitOutside(frames[1], *offset) == 0);
}

// Each code with `mask` cleared and `set` set.
Codes Changed(Codes codes, unsigned mask, unsigned set = 0) {
    for (unsigned& code : codes) {
        code = (code & ~mask) | set;
    }
    return codes;
}

}  // namespace

TEST_CASE("typewriter: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"lookalikes", "wallpaper", "copies", "every_other_line", "zeros",
                             "m_is_return", "giant_letters"}) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "typewriter");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("typewriter: lookalikes", "[machines][slow]") {
    Machine machine = LoadPuzzle("lookalikes");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "HELLO\r");
        CHECK(*PinNet(*t.loaded, "U18", "PHI2") != *PinNet(*t.loaded, "U17", "PHI2"));
        const Codes faded = Changed(Typed("HELLO"), 0x04);  // HAHHK
        CHECK(t.codes() == faded);
        CheckShown(t, faded);
    }
    REQUIRE(machine.swap("U18"));  // the tempting wrong repair: the new chip fades too
    CHECK_FALSE(machine.completed());
    CHECK(Typing(machine, "HELLO\r").codes() == Changed(Typed("HELLO"), 0x04));
    REQUIRE(machine.mend(LookalikesCrack().layer, LookalikesCrack().at));
    CHECK(machine.completed());
    CHECK(Typing(machine, "HELLO\r").codes() == Typed("HELLO"));
}

TEST_CASE("typewriter: wallpaper", "[machines][slow]") {
    Machine machine = LoadPuzzle("wallpaper");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t(1, Build(machine));
        t.clear();
        t.sim.runFor(20_ms);      // a frame: every cell written again
        Codes codes = t.codes();  // before any key, what the undriven keyboard lines read
        CHECK(std::ranges::all_of(codes, [&](unsigned code) { return code == codes[0]; }));
        CHECK(codes[0] != kSpace);
        t.type("A");
        codes.fill('A' & 0x3F);
        CHECK(t.codes() == codes);
        CHECK(t.cursor() == Cell{1, 0});
    }
    REQUIRE(machine.swap("U29"));
    CHECK(machine.completed());
    CHECK(Typing(machine, "A").codes() == Typed("A"));
}

TEST_CASE("typewriter: copies", "[machines][slow]") {
    Machine machine = LoadPuzzle("copies");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "HI\r");
        CHECK(t.codes() == Typed("HI"));  // the page is right
        const Codes shown = t.rowBuffer();
        CHECK(shown != Typed("HI"));
        CheckShown(t, shown);  // the buffer's row, 16 times
    }
    REQUIRE(machine.swap("U22"));
    CHECK(machine.completed());
    Typewriter t = Typing(machine, "HI\r");
    CheckShown(t, Typed("HI"));
}

TEST_CASE("typewriter: every_other_line", "[machines][slow]") {
    Machine machine = LoadPuzzle("every_other_line");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "AB\rCD\rE");
        const Point pin5 = *machine.padPosition("U36", "5");
        CHECK(machine.probeNet(*t.loaded, Layer::Back, pin5) == PinNet(*t.loaded, "U34", "QA"));
        CHECK(*PinNet(*t.loaded, "U36", "2B") != *PinNet(*t.loaded, "U34", "QA"));  // the leg
        Codes expected = Typed("");
        expected[kColumns] = 'C' & 0x3F;  // AB typed on row 1, then CD over it
        expected[kColumns + 1] = 'D' & 0x3F;
        expected[3 * kColumns] = 'E' & 0x3F;
        CHECK(t.codes() == expected);
        CHECK(t.cursor() == Cell{1, 2});  // the counter is right: row 2 types on row 3
    }
    REQUIRE(machine.resolder({"U36", "5"}));
    CHECK(machine.completed());
    Codes expected = Typed("AB");
    expected[kColumns] = 'C' & 0x3F;
    expected[kColumns + 1] = 'D' & 0x3F;
    expected[2 * kColumns] = 'E' & 0x3F;
    CHECK(Typing(machine, "AB\rCD\rE").codes() == expected);
}

TEST_CASE("typewriter: zeros", "[machines][slow]") {
    Machine machine = LoadPuzzle("zeros");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "HELLO\r");
        CHECK(*PinNet(*t.loaded, "U20", "OUT") == *PinNet(*t.loaded, "U20", "READ"));  // P4 on VCC
        CHECK(t.codes() == Typed("HELLO"));               // the page is right
        CheckShown(t, Changed(Typed("HELLO"), 0, 0x10));  // XU\\_ on a page of zeros
    }
    REQUIRE(machine.swap("U20"));  // the tempting wrong repair
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "HELLO\r");
        CheckShown(t, Changed(Typed("HELLO"), 0, 0x10));
    }
    REQUIRE(machine.removeBridge({"U20", "2"}, {"U20", "3"}));
    CHECK(machine.completed());
    Typewriter t = Typing(machine, "HELLO\r");
    CheckShown(t, Typed("HELLO"));
}

TEST_CASE("typewriter: m_is_return", "[machines][slow]") {
    Machine machine = LoadPuzzle("m_is_return");
    CHECK_FALSE(machine.completed());
    {
        Typewriter t = Typing(machine, "AM-B");
        CHECK(*PinNet(*t.loaded, "U30", "A") != *PinNet(*t.loaded, "U12", "2Y"));
        Codes expected = Typed("AM");
        expected[kColumns] = '-' & 0x3F;
        expected[2 * kColumns] = 'B' & 0x3F;
        CHECK(t.codes() == expected);
        CHECK(t.cursor() == Cell{1, 2});
    }
    REQUIRE(machine.mend(MIsReturnCrack().layer, MIsReturnCrack().at));
    CHECK(machine.completed());
    Typewriter t = Typing(machine, "AM-B");
    CHECK(t.codes() == Typed("AM-B"));
    CHECK(t.cursor() == Cell{4, 0});
}

TEST_CASE("typewriter: giant_letters", "[machines][slow]") {
    // X1 at 2 MHz: every count at half speed, each line 128 us: the TV never locks.
    Machine machine = LoadPuzzle("giant_letters");
    CHECK_FALSE(machine.completed());
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO("seed " << seed);
        Typewriter t(seed, Build(machine));
        CHECK(CanPeriod(*t.loaded, "X1") == 500'000);
        CHECK(t.lockedFrames(1).empty());  // none in a second
    }
    REQUIRE(machine.swap("U1"));  // the dot counter: Troubleshooting's first chip
    CHECK_FALSE(machine.completed());
    REQUIRE(machine.swap("X1"));
    CHECK(machine.completed());
    Typewriter t = Typing(machine, "HELLO\r");
    CHECK(t.codes() == Typed("HELLO"));
    CheckShown(t, Typed("HELLO"));
}
