// The BW-2001 firmware on the model (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md, Tests): power-on,
// the editor, BASIC, the monitor and the self-test, typed key by key through the matrix.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "bw2001_harness.hpp"
#include "bw2001_model.hpp"
#include "parts/rom_contents.hpp"

using namespace bw;

namespace {

using Rows = std::vector<std::string>;

const Rows kBanner{"*** BW-2001 TINY BASIC ***", "", "7166 BYTES FREE", "", "READY"};

// A powered model, at READY.
struct Machine {
    Bw2001Model model;
    Bw2001Typist typist;

    explicit Machine(Bw2001Model::Options options = {}) : model(options), typist(model) {
        REQUIRE(typist.waitForLine());
    }
    Rows rows() const { return typist.rows(); }
    std::string row(std::size_t i) const {
        const Rows all = rows();
        return i < all.size() ? all[i] : "";
    }
    void enter(std::string_view text) {
        typist.enter(text);
        REQUIRE(typist.waitForLine());
    }
    void press(Bw2001Key key, bool shift = false) { typist.press(key, shift); }
    void clear() {  // CLR, the cursor top left
        press(kBw2001Home, true);
    }
};

std::vector<std::uint8_t> Rom(std::string_view name, std::size_t size) {
    const auto words = ReadRomContents(std::filesystem::path(BW_ROM_DIR) / name, size, 8);
    REQUIRE(words.has_value());
    return *words;
}

}  // namespace

TEST_CASE("bw2001 roms: each chip sums to 0, each entry point is a JMP", "[bw2001]") {
    struct Window {
        const char* file;
        std::uint16_t base;
        std::size_t size;
    };
    for (const Window w :
         {Window{"bw2001_basic.rom", 0xC000, 0x2000}, Window{"bw2001_edit.rom", 0xE000, 0x800},
          Window{"bw2001_kernel.rom", 0xF000, 0x1000}}) {
        INFO(w.file);
        const auto words = Rom(w.file, w.size);
        for (std::size_t chip = 0; chip < w.size; chip += 0x800) {
            unsigned sum = 0;
            for (std::size_t i = chip; i < chip + 0x800; ++i) {
                sum += words[i];
            }
            INFO("chip at " << std::hex << (w.base + chip));
            CHECK((sum & 0xFF) == 0);
        }
    }
    const auto basic = Rom("bw2001_basic.rom", 0x2000);
    const auto edit = Rom("bw2001_edit.rom", 0x800);
    const auto kernel = Rom("bw2001_kernel.rom", 0x1000);
    for (const std::uint16_t a : {0xC000, 0xC003, 0xC006}) {
        CHECK(basic[a - 0xC000] == 0x4C);
    }
    for (std::uint16_t a = 0xE000; a <= 0xE00F; a += 3) {
        CHECK(edit[a - 0xE000] == 0x4C);
    }
    for (const std::uint16_t a : {0xF000, 0xF003, 0xF800, 0xF803}) {
        CHECK(kernel[a - 0xF000] == 0x4C);
    }
}

TEST_CASE("bw2001 roms: the editor's KEYTAB is the harness's matrix", "[bw2001]") {
    const auto edit = Rom("bw2001_edit.rom", 0x800);
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 8; ++column) {
            const std::string_view legend = Bw2001KeyLegend({row, column});
            const std::uint8_t code = edit[0x700 + row * 8 + column];
            INFO("row " << row << " column " << column << " " << legend);
            if (legend.size() == 1) {
                CHECK(code == static_cast<std::uint8_t>(legend[0]));
            } else if (legend == "SPACE") {
                CHECK(code == 0x20);
            } else if (legend == "RETURN") {
                CHECK(code == 0x0D);
            } else if (legend == "STOP") {
                CHECK(code == 0x03);
            } else if (legend == "CRSR DN") {
                CHECK(code == 0x11);
            } else if (legend == "CRSR RT") {
                CHECK(code == 0x1D);
            } else if (legend == "HOME") {
                CHECK(code == 0x13);
            } else if (legend == "DEL") {
                CHECK(code == 0x14);
            } else if (legend == "RVS") {
                CHECK(code == 0x12);
            } else {
                CHECK(code == 0);  // SHIFT, unused
            }
        }
    }
}

TEST_CASE("bw2001 power-on: the banner, READY and the RAM sized", "[bw2001]") {
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO("seed " << seed);
        Machine m({.seed = seed});
        CHECK(m.rows() == kBanner);
        CHECK(m.model.peek(0x60) == 0x00);
        CHECK(m.model.peek(0x61) == 0x20);
        CHECK_FALSE(m.model.touchedUnmodelled());
    }
    Machine small({.ramTop = 0x1800});
    CHECK(small.row(2) == "5118 BYTES FREE");
    Bw2001Model stuck({});
    stuck.stick(0x1234, 0, true);
    stuck.reset();
    Bw2001Typist typist(stuck);
    REQUIRE(typist.waitForLine());
    CHECK(typist.rows()[2] == "3634 BYTES FREE");
}

TEST_CASE("bw2001 editor: typed text echoes, RETURN hands the line over", "[bw2001]") {
    Machine m;
    m.clear();
    m.enter("HELLO, 12+3");
    CHECK(m.rows() == Rows{"HELLO, 12+3", "=HELLO, 12+3"});
    // STOP while the editor waits for a line is discarded.
    m.press(kBw2001Stop);
    m.enter("A");
    CHECK(m.row(3) == "=A");
}

TEST_CASE("bw2001 editor: the cursor keys move without changing anything", "[bw2001]") {
    Machine m;
    m.clear();
    m.typist.type("ABC");
    m.press(kBw2001CursorRight, true);  // left
    m.press(kBw2001CursorRight, true);
    m.typist.type("X");
    m.enter("");
    CHECK(m.row(0) == "AXC");
    CHECK(m.row(1) == "=AXC");
    // Up onto the first row, right to its end: RETURN enters that row again.
    m.press(kBw2001CursorDown, true);  // up, to row 1
    m.press(kBw2001CursorDown, true);  // row 0
    m.press(kBw2001CursorRight);
    m.enter("");
    CHECK(m.row(1) == "=AXC");  // the row after row 0 is written over
    CHECK(m.row(0) == "AXC");
}

TEST_CASE("bw2001 editor: DEL and INST", "[bw2001]") {
    Machine m;
    m.clear();
    m.typist.type("ABCD");
    m.press(kBw2001CursorRight, true);
    m.press(kBw2001CursorRight, true);  // on C
    m.press(kBw2001Del);                // B goes
    CHECK(m.row(0) == "ACD");
    m.press(kBw2001Del, true);  // INST at C
    CHECK(m.row(0) == "A CD");
    m.typist.type("X");
    m.enter("");
    CHECK(m.row(1) == "=AXCD");
    // DEL at the start of a line does nothing.
    m.press(kBw2001Del);
    m.enter("Q");
    CHECK(m.row(2) == "Q");
}

TEST_CASE("bw2001 editor: a long line joins two rows; 64 characters at most", "[bw2001]") {
    Machine m;
    m.clear();
    const std::string fifty = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMN";
    m.enter(fifty);
    CHECK(m.row(0) == fifty.substr(0, 40));
    CHECK(m.row(1) == fifty.substr(40));
    CHECK(m.row(2) == "=" + fifty.substr(0, 39));
    CHECK(m.row(3) == fifty.substr(39));
    // Up into the first row of the typed line: RETURN takes both rows.
    for (int i = 0; i < 4; ++i) {
        m.press(kBw2001CursorDown, true);
    }
    m.enter("");
    CHECK(m.row(2) == "=" + fifty.substr(0, 39));
    m.clear();
    const std::string seventy = fifty + "OPQRSTUVWXYZ01234567";
    m.enter(seventy);
    CHECK(m.row(0) == seventy.substr(0, 40));
    CHECK(m.row(1) == seventy.substr(40, 24));
    CHECK(m.row(2) == "=" + seventy.substr(0, 39));
    CHECK(m.row(3) == seventy.substr(39, 25));
}

TEST_CASE("bw2001 editor: the screen scrolls; HOME and CLR", "[bw2001][slow]") {
    Machine m;
    for (int i = 10; i < 25; ++i) {
        m.enter(std::to_string(i));
    }
    // 5 banner rows, 15 lines and 15 echoes, the cursor's row: 36 rows, so 11 scrolled away.
    const Rows rows = m.rows();
    REQUIRE(rows.size() == 24);
    CHECK(rows[0] == "13");
    CHECK(rows[22] == "24");
    CHECK(rows[23] == "=24");
    m.press(kBw2001Home);
    m.enter("H");
    CHECK(m.row(0) == "H3");
    CHECK(m.row(1) == "=H3");
    m.clear();
    CHECK(m.rows().empty());
}
