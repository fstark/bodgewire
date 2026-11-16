#include "parts/zx80_keys.hpp"

#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::size_t Key(std::string_view legend) {
    const auto keys = Zx80Keys();
    for (std::size_t k = 0; k < keys.size(); ++k) {
        if (keys[k].legend == legend) {
            return k;
        }
    }
    FAIL("no key " << legend);
    return 0;
}

// The presses as "P", "+K" (with SHIFT), space-separated.
std::string Spelled(const std::vector<MatrixPress>& presses) {
    std::string out;
    for (const MatrixPress& p : presses) {
        if (!out.empty()) {
            out += ' ';
        }
        if (p.shift) {
            out += '+';
        }
        const std::string_view legend = Zx80Keys()[p.key].legend;
        out += legend == "NEW LINE" ? "NL" : legend == "SPACE" ? "SP" : std::string(legend);
    }
    return out;
}

}  // namespace

TEST_CASE("zx80 keys: the table", "[parts]") {
    const auto keys = Zx80Keys();
    REQUIRE(keys.size() == kZx80KeyCount);
    bool seen[8][5]{};
    for (const Zx80Key& key : keys) {
        REQUIRE(key.row < 8);
        REQUIRE(key.column < 5);
        CHECK_FALSE(seen[key.row][key.column]);
        seen[key.row][key.column] = true;
    }
    CHECK(Zx80KeyAt(0, 0) == Key("SHIFT"));
    CHECK(Zx80KeyAt(6, 0) == Key("NEW LINE"));
    CHECK(Zx80KeyAt(7, 0) == Key("SPACE"));
    CHECK(Zx80KeyAt(4, 0) == Key("0"));
    CHECK(Zx80KeyAt(5, 1) == Key("O"));
    CHECK(keys[Key("O")].keyword == "PRINT");
    CHECK(keys[Key("Y")].shifted == "\"");
    CHECK(keys[Key("P")].keyword.empty());
    CHECK_FALSE(Zx80KeyAt(8, 0).has_value());
}

TEST_CASE("zx80 keys: the character map", "[parts]") {
    const auto press = [](char32_t c) { return Zx80KeyFor(c); };
    REQUIRE(press('a'));
    CHECK(press('a')->key == Key("A"));
    CHECK_FALSE(press('a')->shift);
    CHECK(press('7')->key == Key("7"));
    CHECK(press('.')->key == Key("."));
    CHECK(press(' ')->key == Key("SPACE"));
    CHECK(press('\n')->key == Key("NEW LINE"));
    CHECK(press('"')->key == Key("Y"));
    CHECK(press('"')->shift);
    CHECK(press('+')->key == Key("K"));
    CHECK(press('*')->key == Key("P"));
    CHECK(press(',')->key == Key("."));
    CHECK(press(',')->shift);
    CHECK(press(kPoundSign)->key == Key("SPACE"));
    CHECK(press(kPoundSign)->shift);
    CHECK(press('\b')->key == Key("0"));
    CHECK(press('\b')->shift);
    CHECK(press(kKeyDel)->key == Key("0"));
    CHECK(press(kKeyHome)->key == Key("9"));
    CHECK(press(kKeyLeft)->key == Key("5"));
    CHECK(press(kKeyRight)->key == Key("8"));
    CHECK(press(kKeyUp)->key == Key("7"));
    CHECK(press(kKeyDown)->key == Key("6"));
    CHECK(press(kKeyEdit)->key == Key("NEW LINE"));
    CHECK(press(kKeyEdit)->shift);
    CHECK(press(kKeyBreak)->key == Key("SPACE"));
    CHECK_FALSE(press(kKeyBreak)->shift);
    CHECK(Zx80Keys()[Key("SPACE")].keyword == "BREAK");
    CHECK_FALSE(press('{'));
    CHECK_FALSE(press('!'));
    CHECK_FALSE(press('\t'));
    for (const char* symbols = "\"$()*-+=:;?/<>,"; *symbols != '\0'; ++symbols) {
        INFO("symbol " << *symbols);
        REQUIRE(press(static_cast<char32_t>(*symbols)));
        CHECK(press(static_cast<char32_t>(*symbols))->shift);
    }
}

TEST_CASE("zx80 keys: a listing is typed keyword by keyword", "[parts]") {
    CHECK(Spelled(Zx80Presses(U"10 PRINT \"HI\"\n")) == "1 0 O +Y H I +Y NL");
    CHECK(Spelled(Zx80Presses(U"20 GO TO 10\n")) == "2 0 G 1 0 NL");
    CHECK(Spelled(Zx80Presses(U"goto 10")) == "G 1 0");
    CHECK(Spelled(Zx80Presses(U"GO SUB 100")) == "V 1 0 0");
    CHECK(Spelled(Zx80Presses(U"RETURN")) == "B");
    CHECK(Spelled(Zx80Presses(U"IF A=1 THEN PRINT A**2")) == "U A +L 1 +3 O A +H 2");
    CHECK(Spelled(Zx80Presses(U"FOR I=1 TO 5")) == "F I +L 1 +4 5");
    CHECK(Spelled(Zx80Presses(U"LET A=PEEK(16396)")) == "K A +L P E E K +I 1 6 3 9 6 +O");
    CHECK(Spelled(Zx80Presses(U"PRINT NOT A AND B OR C")) == "O +1 A +2 B +B C");
    // A string keeps its spaces; a word in it is letters.
    CHECK(Spelled(Zx80Presses(U"PRINT \"GO TO\"")) == "O +Y G O SP T O +Y");
    CHECK(Spelled(Zx80Presses(U"REM A B")) == "Y A SP B");
    // Not a keyword at a line's start: typed as letters (the editor would make the first one a
    // keyword, but the listing is wrong, not the typist).
    CHECK(Spelled(Zx80Presses(U"HELLO")) == "H E L L O");
    CHECK(Spelled(Zx80Presses(U"RUN\n")) == "R NL");
    CHECK(Zx80Presses(U"{}").empty());
}
