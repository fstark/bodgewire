// The BW-2001 firmware on the model (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md, Tests): power-on,
// the editor, BASIC, the monitor and the self-test, typed key by key through the matrix.

#include <cstdint>
#include <filesystem>
#include <span>
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
        model.run(300'000);  // the clear waits for the retrace: about nine frames (P5q)
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
        CHECK(m.model.viaPcr() == 0x0C);     // CA2 low: the upper-case character set
        CHECK(m.model.snowAccesses() == 0);  // the screen only in the retrace (P5q)
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

TEST_CASE("bw2001 editor: typed text echoes, RETURN hands the line to BASIC", "[bw2001]") {
    Machine m;
    m.clear();
    m.enter("PRINT 12+3");
    CHECK(m.rows() == Rows{"PRINT 12+3", "15", "READY"});
    // STOP while the editor waits for a line is discarded.
    m.press(kBw2001Stop);
    m.enter("PRINT 1");
    CHECK(m.row(4) == "1");
    CHECK(m.row(5) == "READY");
}

TEST_CASE("bw2001 editor: the cursor keys move without changing anything", "[bw2001]") {
    Machine m;
    m.clear();
    m.typist.type("10 ABC");
    m.press(kBw2001CursorRight, true);  // left
    m.press(kBw2001CursorRight, true);
    m.typist.type("X");
    m.enter("");
    m.enter("LIST");
    CHECK(m.rows() == Rows{"10 AXC", "LIST", "10 AXC", "READY"});
    // Up onto row 0, right, down past it: nothing changes; RETURN there stores it again.
    for (int i = 0; i < 4; ++i) {
        m.press(kBw2001CursorDown, true);
    }
    m.press(kBw2001CursorRight);
    m.press(kBw2001CursorRight);
    m.press(kBw2001CursorDown);
    m.press(kBw2001CursorDown, true);
    CHECK(m.rows() == Rows{"10 AXC", "LIST", "10 AXC", "READY"});
    m.enter("");
    m.press(kBw2001Home, true);
    m.enter("LIST");
    CHECK(m.rows() == Rows{"LIST", "10 AXC", "READY"});
}

TEST_CASE("bw2001 editor: DEL and INST", "[bw2001]") {
    Machine m;
    m.clear();
    m.typist.type("10 ABCD");
    m.press(kBw2001CursorRight, true);
    m.press(kBw2001CursorRight, true);  // on C
    m.press(kBw2001Del);                // B goes
    CHECK(m.row(0) == "10 ACD");
    m.press(kBw2001Del, true);  // INST at C
    CHECK(m.row(0) == "10 A CD");
    m.typist.type("X");
    m.enter("");
    m.enter("LIST");
    CHECK(m.row(2) == "10 AXCD");
    // DEL at the start of a line does nothing.
    m.press(kBw2001Del);
    m.enter("PRINT 7");
    CHECK(m.row(4) == "PRINT 7");
    CHECK(m.row(5) == "7");
}

TEST_CASE("bw2001 editor: a long line joins two rows; 64 characters at most", "[bw2001][slow]") {
    Machine m;
    m.clear();
    const std::string fifty = "10 REM ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFG";
    REQUIRE(fifty.size() == 50);
    m.enter(fifty);
    CHECK(m.row(0) == fifty.substr(0, 40));
    CHECK(m.row(1) == fifty.substr(40));
    m.enter("LIST");
    CHECK(m.row(3) == fifty.substr(0, 40));
    CHECK(m.row(4) == fifty.substr(40));
    // RETURN on the second row of a listed line takes both rows: a change there is stored.
    for (int i = 0; i < 2; ++i) {
        m.press(kBw2001CursorDown, true);
    }
    m.typist.type("Z");
    m.enter("");
    m.press(kBw2001Home, true);
    m.enter("LIST");
    CHECK(m.row(1) == fifty.substr(0, 40));
    CHECK(m.row(2) == "Z" + fifty.substr(41));
    m.clear();
    const std::string seventy = "20 REM " + std::string(63, 'Q');
    m.enter(seventy);
    CHECK(m.row(0) == seventy.substr(0, 40));
    CHECK(m.row(1) == seventy.substr(40, 24));
    m.enter("LIST 20");
    CHECK(m.row(3) == seventy.substr(0, 40));
    CHECK(m.row(4) == seventy.substr(40, 24));
}

TEST_CASE("bw2001 editor: the screen scrolls; HOME and CLR", "[bw2001][slow]") {
    Machine m;
    for (int i = 10; i < 25; ++i) {
        m.enter("PRINT " + std::to_string(i));
    }
    // 5 banner rows, 15 entries of 3 rows, the cursor's row: 51 rows, so 26 scrolled away.
    const Rows rows = m.rows();
    REQUIRE(rows.size() == 24);
    CHECK(rows[0] == "PRINT 17");
    CHECK(rows[21] == "PRINT 24");
    CHECK(rows[22] == "24");
    CHECK(rows[23] == "READY");
    m.press(kBw2001Home);
    m.enter("PRINT 9");  // over "PRINT 17": the row is "PRINT 97"
    CHECK(m.row(0) == "PRINT 97");
    CHECK(m.row(1) == "97");
    CHECK(m.row(2) == "READY");
    m.clear();
    CHECK(m.rows().empty());
}

TEST_CASE("bw2001 editor: a listing scrolls with its links", "[bw2001][slow]") {
    Machine m;
    // 14 lines of two rows each: the listing scrolls the screen.
    for (int n = 1; n <= 14; ++n) {
        m.enter(std::to_string(n * 10 + 100) + " REM " +
                std::string(40, static_cast<char>('A' + n)));
    }
    m.clear();
    m.enter("LIST");
    // The last line listed (240) is on the two rows above READY; edit its second row in place.
    const Rows rows = m.rows();
    REQUIRE(rows.size() >= 3);
    CHECK(rows[rows.size() - 1] == "READY");
    CHECK(rows[rows.size() - 3] == "240 REM " + std::string(32, 'O'));
    CHECK(rows[rows.size() - 2] == std::string(8, 'O'));
    m.press(kBw2001CursorDown, true);
    m.press(kBw2001CursorDown, true);
    m.typist.type("X");
    m.enter("");
    m.clear();
    m.enter("LIST 240");
    CHECK(m.row(1) == "240 REM " + std::string(32, 'O'));
    CHECK(m.row(2) == "X" + std::string(7, 'O'));
}

TEST_CASE("bw2001 editor: INPUT takes only what is right of its prompt", "[bw2001]") {
    Machine m;
    m.clear();
    m.enter("10 INPUT \"N\";A");
    m.enter("20 PRINT A*2");
    m.enter("RUN");
    CHECK(m.row(3) == "N?");
    m.enter("21");
    CHECK(m.row(3) == "N? 21");
    CHECK(m.row(4) == "42");
    CHECK(m.row(5) == "READY");
}

TEST_CASE("bw2001 conformance", "[bw2001][slow]") {
    for (const std::filesystem::path& path : Bw2001Sessions()) {
        DYNAMIC_SECTION(path.filename().string()) {
            Bw2001Model model({});
            REQUIRE(Bw2001Typist(model).waitForLine());
            model.resetStackMark();  // the kernel's reset is not BASIC's
            const auto difference = RunBw2001Session(model, path);
            INFO(difference.value_or(""));
            CHECK_FALSE(difference.has_value());
            CHECK_FALSE(model.touchedUnmodelled());
            CHECK(model.snowAccesses() == 0);  // printing, scrolling, the cursor, RETURN (P5q)
            // BASIC's deepest stack use (design §6.6): about 200 bytes at P5o's depth of 6.
            UNSCOPED_INFO("lowest stack byte written $1" << std::hex << int(model.stackMark()));
            CHECK(model.stackMark() >= 0x20);  // $0120 and above
        }
    }
}

TEST_CASE("bw2001 basic: USR, OUT OF MEMORY", "[bw2001][slow]") {
    {
        Machine m;
        m.clear();
        m.enter("POKE 1008,169:POKE 1009,52:POKE 1010,160");
        m.enter("POKE 1011,18:POKE 1012,96");
        m.clear();
        m.enter("PRINT USR(1008)");
        CHECK(m.rows() == Rows{"PRINT USR(1008)", "4660", "READY"});
    }
    Machine m({.ramTop = 0x0500});
    CHECK(m.row(2) == "254 BYTES FREE");
    m.clear();
    // Each a 36-byte record: 7 fit in 254 bytes, the 8th does not.
    for (int n = 1; n <= 7; ++n) {
        m.enter(std::to_string(n * 10) + " REM " + std::string(30, 'A'));
    }
    m.clear();
    m.enter("80 REM " + std::string(30, 'B'));
    CHECK(m.rows() == Rows{"80 REM " + std::string(30, 'B'), "OUT OF MEMORY ERROR", "READY"});
    // A longer text for an existing line: refused, the old one kept.
    m.clear();
    m.enter("10 REM " + std::string(34, 'C'));  // 41 characters: two rows
    CHECK(m.row(2) == "OUT OF MEMORY ERROR");
    m.clear();
    m.enter("LIST 10");
    CHECK(m.row(1) == "10 REM " + std::string(30, 'A'));
    m.clear();
    m.enter("LIST 80");
    CHECK(m.rows() == Rows{"LIST 80", "READY"});
}

namespace {

// The fields of the monitor's ".; pppp ss aa xx yy ss" row.
std::vector<std::string> Registers(const std::string& row) {
    std::vector<std::string> fields;
    std::size_t at = 3;
    while (at < row.size()) {
        const std::size_t end = row.find(' ', at);
        fields.push_back(row.substr(at, end == std::string::npos ? std::string::npos : end - at));
        at = end == std::string::npos ? row.size() : end + 1;
    }
    return fields;
}

}  // namespace

TEST_CASE("bw2001 monitor: USR(61440), the registers, .G returning a value", "[bw2001]") {
    Machine m;
    m.clear();
    m.enter("PRINT USR(61440)");
    Rows rows = m.rows();
    REQUIRE(rows.size() == 5);
    CHECK(rows[1] == "C*");
    CHECK(rows[2] == "   PC  SR AC XR YR SP");
    REQUIRE(rows[3].starts_with(".; "));
    CHECK(rows[4] == ".");
    const auto fields = Registers(rows[3]);
    REQUIRE(fields.size() == 6);
    // AC and YR set to $34 and $12: USR returns $1234.
    m.enter("; " + fields[0] + " " + fields[1] + " 34 " + fields[3] + " 12 " + fields[5]);
    m.enter("R");
    CHECK(m.row(7) ==
          ".; " + fields[0] + " " + fields[1] + " 34 " + fields[3] + " 12 " + fields[5]);
    m.enter("G");
    rows = m.rows();
    REQUIRE(rows.size() >= 2);
    CHECK(rows[rows.size() - 2] == "4660");
    CHECK(rows.back() == "READY");
    // A bad command.
    m.clear();
    m.enter("PRINT USR(61440)");
    m.enter("Q");
    CHECK(m.row(5) == "?");
    CHECK(m.row(6) == ".");
}

TEST_CASE("bw2001 monitor: .M dumps, a dump row edited in place, .:, .X keeps the program",
          "[bw2001][slow]") {
    Machine m;
    m.clear();
    m.enter("10 PRINT A");
    m.enter("PRINT USR(61440)");
    m.enter("M 0400 0407");
    CHECK(m.row(6) == ".: 0400 0A 00 07 80 20 41 00 00");
    m.enter("M 03F0 03F0");
    CHECK(m.row(8) == ".: 03F0 00 00 00 00 00 00 00 00");
    // Up onto the dump row, right to its first byte, overtype it, RETURN.
    m.press(kBw2001CursorDown, true);
    for (int i = 0; i < 7; ++i) {
        m.press(kBw2001CursorRight);
    }
    m.typist.type("AB");
    m.enter("");
    m.enter(": 03F1 CD EF");
    m.enter("X");
    m.clear();
    m.enter("PRINT PEEK(1008);PEEK(1009);PEEK(1010)");
    CHECK(m.row(1) == "171205239");
    m.enter("LIST");
    CHECK(m.row(4) == "10 PRINT A");
}

TEST_CASE("bw2001 monitor: a BRK shows B* and its PC; .G goes on after it", "[bw2001]") {
    Machine m;
    m.clear();
    // LDA #7, BRK, (its pad), LDY #0, RTS at 1008 ($03F0).
    m.enter("POKE 1008,169:POKE 1009,7:POKE 1010,0");
    m.enter("POKE 1011,234:POKE 1012,160:POKE 1013,0:POKE 1014,96");
    m.clear();
    m.enter("PRINT USR(1008)");
    const Rows rows = m.rows();
    REQUIRE(rows.size() == 5);
    CHECK(rows[1] == "B*");
    const auto fields = Registers(rows[3]);
    REQUIRE(fields.size() == 6);
    CHECK(fields[0] == "03F4");
    CHECK(fields[2] == "07");
    m.enter("G");
    CHECK(m.row(5) == "7");
    CHECK(m.row(6) == "READY");
}

namespace {

const Rows kSelfTest{"BW-2001 SELF-TEST",
                     "",
                     "ZERO PAGE OK",
                     "VIDEO RAM OK",
                     "RAM OK",
                     "ROM C000 OK",
                     "ROM C800 OK",
                     "ROM E000 OK",
                     "ROM F000 OK",
                     "ROM F800 OK",
                     "",
                     "KEY"};

// The self-test's rows after `us` (the KEY row comes at about 0.8 s).
Rows SelfTest(Bw2001Model& model, std::uint64_t us = 1'200'000) {
    model.run(us);
    return Bw2001Typist(model).rows();
}

}  // namespace

TEST_CASE("bw2001 self-test: DIAG at reset, every row OK, the held key", "[bw2001][slow]") {
    Bw2001Model model({.diag = true});
    CHECK(SelfTest(model) == kSelfTest);
    struct Held {
        Bw2001Key key;
        const char* row;
    };
    for (const Held h : {Held{kBw2001Return, "KEY 65 RETURN"}, Held{{4, 0}, "KEY 40 A"},
                         Held{kBw2001ShiftLeft, "KEY 80 SHIFT L"}, Held{kBw2001Stop, "KEY 94 STOP"},
                         Held{{9, 2}, "KEY 92 SPACE"}}) {
        model.setKey(h.key, true);
        CHECK(SelfTest(model, 20'000)[11] == h.row);
        model.setKey(h.key, false);
    }
    CHECK(SelfTest(model, 20'000)[11] == "KEY");
    CHECK_FALSE(model.touchedUnmodelled());
}

TEST_CASE("bw2001 self-test: from the monitor's .T", "[bw2001][slow]") {
    Machine m;
    m.enter("PRINT USR(61440)");
    m.typist.enter("T");
    CHECK(SelfTest(m.model) == kSelfTest);
}

TEST_CASE("bw2001 self-test: each failure on its row", "[bw2001][slow]") {
    {
        Bw2001Model model({.diag = true});
        model.stick(0x0042, 7, true);  // pass 2 writes $42 ^ $5A = $18
        model.reset();
        const Rows rows = SelfTest(model, 100'000);
        CHECK(rows == Rows{"BW-2001 SELF-TEST", "", "ZERO PAGE BAD 0042 80"});
    }
    {
        Bw2001Model model({.diag = true});
        model.deadRam();
        model.reset();
        CHECK(SelfTest(model, 100'000)[2] == "ZERO PAGE BAD 0000 5A");
    }
    {
        Bw2001Model model({.diag = true});
        model.stick(0x8123, 3, true);  // pass 1 writes $23 ^ $81 ^ $A5 = $07
        model.reset();
        const Rows rows = SelfTest(model, 300'000);
        CHECK(rows == Rows{"BW-2001 SELF-TEST", "", "ZERO PAGE OK", "VIDEO RAM BAD 8123 08"});
    }
    {
        Bw2001Model model({.diag = true});
        model.stick(0x1234, 0, false);  // pass 1 writes $34 ^ $12 ^ $A5 = $83
        model.reset();
        const Rows rows = SelfTest(model);
        REQUIRE(rows.size() == 5);
        CHECK(rows[4] == "RAM BAD 1234 01");
    }
    {
        Bw2001Model model({.diag = true});
        const std::uint8_t one = static_cast<std::uint8_t>(model.peek(0xC100) + 1);
        model.patchRom(0xC100, std::span(&one, 1));
        model.reset();
        Rows expected = kSelfTest;
        expected[5] = "ROM C000 BAD 01";
        CHECK(SelfTest(model) == expected);
    }
}
