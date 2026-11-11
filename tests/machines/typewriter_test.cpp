// 090 TV typewriter (docs/machines/090-typewriter/TYPEWRITER.md): the page RAMs on the TV.

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

#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "parts/button.hpp"
#include "parts/keyboard.hpp"
#include "parts/ram2114.hpp"
#include "parts/rom_contents.hpp"
#include "sim/simulator.hpp"

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
        auto read = ReadRomContents(std::filesystem::path(BW_ROOT_DIR) / "roms/2513.rom", 512, 5);
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

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "typewriter.bwn");
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Machine on a simulator, powered on.
struct Typewriter {
    std::unique_ptr<LoadedCircuit> loaded = DesignCircuit();
    Simulator sim{loaded->circuit};

    explicit Typewriter(std::uint64_t seed) {
        sim.powerOn(seed);
        sim.runFor(1_ms);
    }

    // Clear (SW1) held for 25 ms, more than a frame, then released.
    void clear() {
        Button* button = ButtonOf(*loaded, "SW1");
        button->setPressed(true);
        sim.wake(button->id());
        sim.runFor(25_ms);
        button->setPressed(false);
        sim.wake(button->id());
        sim.runFor(1_ms);
    }

    // Each code typed on J3 in turn, 250 µs apart (written and stepped within 79 µs, the next key
    // 101 µs after a strobe); `queued`: all handed over at once, then 250 µs a key.
    void type(const std::vector<std::uint8_t>& keys, bool queued = false) {
        Keyboard* keyboard = KeyboardOf(*loaded, "J3");
        for (const std::uint8_t key : keys) {
            REQUIRE(keyboard->type(key));
            sim.wake(keyboard->id());
            if (!queued) {
                sim.runFor(250_us);
            }
        }
        if (queued) {
            sim.runFor(250_us * static_cast<Time>(keys.size()));
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

    Codes codes() const {
        const auto& low = static_cast<const Ram2114&>(*loaded->components.at("U17"));
        const auto& high = static_cast<const Ram2114&>(*loaded->components.at("U18"));
        Codes codes{};
        for (std::size_t i = 0; i < codes.size(); ++i) {
            codes[i] = low.word(i) | (high.word(i) & 0b11u) << 4;
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

TEST_CASE("typewriter: the TV shows the RAMs", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1, 2);
    INFO("seed " << seed);
    Typewriter t(seed);
    // The first locked frame may have started before the counters fell into step.
    const auto frames = t.lockedFrames(3);
    REQUIRE(frames.size() == 3);
    const auto cursor = t.cursor();
    REQUIRE(cursor.has_value());
    const auto offset = ShownAt(frames[1], t.codes(), *cursor);
    REQUIRE(offset.has_value());
    INFO("left " << offset->left << ", top " << offset->top);
    CHECK(offset->left >= 30);
    CHECK(offset->left <= 38);
    CHECK(offset->top <= 8);
    CHECK(LitOutside(frames[1], *offset) == 0);
    CHECK(ShownAt(frames[2], t.codes(), *cursor).has_value());
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

TEST_CASE("typewriter: lines and the page wrap", "[machines]") {
    Typewriter t(3);
    t.clear();
    const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFG";  // 33
    t.type(letters);
    CHECK(t.codes() == Typed(letters));
    CHECK(t.cursor() == Cell{1, 1});
    t.clear();
    t.type(std::string(16, '\r'));
    CHECK(t.cursor() == Cell{0, 0});
    t.type("Z");
    CHECK(t.codes() == Typed("Z"));
    CHECK(t.cursor() == Cell{1, 0});
}

TEST_CASE("typewriter: keys typed at once all land", "[machines]") {
    Typewriter t(4);
    t.clear();
    const std::string keys = "THE QUICK BROWN FOX JUMPS OVER T";  // 32
    t.type(std::vector<std::uint8_t>(keys.begin(), keys.end()), true);
    CHECK(t.codes() == Typed(keys));
    CHECK(t.cursor() == Cell{0, 1});
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
