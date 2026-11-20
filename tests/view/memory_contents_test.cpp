#include "view/memory_contents.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "parts/rom_contents.hpp"

using namespace bw;

namespace {

std::vector<std::uint8_t> Rom(std::string_view machine, std::string_view file, std::size_t size,
                              int width) {
    auto words = ReadRomContents(test::RomPath(machine, file), size, width);
    REQUIRE(words.has_value());
    return *words;
}

ContentsDefaults Defaults(std::string_view machine) {
    std::ifstream in(test::RomPath(machine, "contents.txt"));
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto defaults = ParseContentsDefaults(text, "contents.txt");
    REQUIRE(defaults.has_value());
    return *defaults;
}

}  // namespace

TEST_CASE("contents: each chip's first view and settings", "[contents]") {
    const ContentsSettings ic2 = DefaultSettings(Defaults("bw80"), "IC2", 8);
    CHECK(ic2.view == ContentsView::Code);
    CHECK(ic2.cpu == Cpu::Z80);
    CHECK(ic2.charset == CharacterSet::Zx80);
    CHECK(ic2.address == 0);
    CHECK(ic2.glyphFrom == 0xE00);
    CHECK(ic2.codeLength == 0);  // the whole chip
    const ContentsSettings ua2 = DefaultSettings(Defaults("bw2001"), "UA2", 8);
    CHECK(ua2.view == ContentsView::Glyphs);
    CHECK(ua2.charset == CharacterSet::Petscii);
    CHECK(ua2.glyphRows == 8);
    CHECK(ua2.glyphBits == 8);
    const ContentsSettings uh4 = DefaultSettings(Defaults("bw2001"), "UH4", 8);
    CHECK(uh4.view == ContentsView::Code);
    CHECK(uh4.cpu == Cpu::Mos6502);
    CHECK(uh4.address == 0xF000);
    const ContentsSettings u15 = DefaultSettings(Defaults("message"), "U15", 5);
    CHECK(u15.view == ContentsView::Glyphs);
    CHECK(u15.glyphBits == 5);
    const ContentsSettings u14 = DefaultSettings(Defaults("message"), "U14", 8);
    CHECK(u14.view == ContentsView::Hex);
    CHECK(u14.charset == CharacterSet::Ascii);
    CHECK(u14.cpu == Cpu::Mos6502);  // no CPU on the Message: the 6502 offered
    CHECK(DefaultSettings(Defaults("bw1"), "U3", 8).address == 0x1800);
    CHECK(DefaultSettings(Defaults("led_computer"), "U2", 8).view == ContentsView::Code);
    CHECK(DefaultSettings(Defaults("typewriter"), "U23", 5).view == ContentsView::Glyphs);
}

TEST_CASE("contents: sizes and character sets", "[contents]") {
    CHECK(SizeText(2048, 8) == "2K x 8");
    CHECK(SizeText(512, 5) == "512 x 5");
    CHECK(SizeText(4096, 8) == "4K x 8");

    CHECK(CharacterOf(CharacterSet::Ascii, 0x1F) == ".");
    CHECK(CharacterOf(CharacterSet::Ascii, 0x20) == " ");
    CHECK(CharacterOf(CharacterSet::Ascii, 'a') == "a");
    CHECK(CharacterOf(CharacterSet::Ascii, 0x7E) == "~");
    CHECK(CharacterOf(CharacterSet::Ascii, 0x7F) == ".");
    CHECK(CharacterOf(CharacterSet::Ascii, 0xC1) == ".");

    CHECK(CharacterOf(CharacterSet::Petscii, 'A') == "A");
    CHECK(CharacterOf(CharacterSet::Petscii, 0x5C) == "\xC2\xA3");
    CHECK(CharacterOf(CharacterSet::Petscii, 0x5D) == "]");
    CHECK(CharacterOf(CharacterSet::Petscii, 0x5E) == ".");
    CHECK(CharacterOf(CharacterSet::Petscii, 0x61) == ".");
    CHECK(CharacterOf(CharacterSet::Petscii, 0xC1) == ".");  // a keyword's last letter

    CHECK(CharacterOf(CharacterSet::Zx80, 0x00) == " ");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x01) == "\"");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x02) == ".");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x0B) == ".");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x0C) == "\xC2\xA3");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x0D) == "$");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x0F) == "?");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x10) == "(");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x1B) == ".");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x1C) == "0");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x25) == "9");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x26) == "A");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x30) == "K");
    CHECK(CharacterOf(CharacterSet::Zx80, 0x3F) == "Z");
    CHECK(CharacterOf(CharacterSet::Zx80, 0xB0) == "K");  // inverse
    CHECK(CharacterOf(CharacterSet::Zx80, 0x40) == ".");  // a token
    CHECK(CharacterOf(CharacterSet::Zx80, 0xD5) == ".");
}

TEST_CASE("contents: hex rows", "[contents]") {
    CHECK(HexColumns(62) == 8);
    CHECK(HexColumns(71) == 16);
    CHECK(HexColumns(38) == 4);
    const std::vector<std::uint8_t> words{0x48, 0x49, 0x00, 0x7E, 0xFF};
    CHECK(HexLine(words, 0xC000, 0, 4, CharacterSet::Ascii) == "C000  48 49 00 7E  HI.~");
    CHECK(HexLine(words, 0xFFFE, 4, 4, CharacterSet::Ascii) == "0002  FF           .");
}

TEST_CASE("contents: the code view's lines", "[contents]") {
    ContentsSettings s;
    s.address = 0x1C00;
    const std::vector<std::uint8_t> words{0xA9, 0x00, 0xD0, 0xFC, 0xFF, 0x20, 0x00};
    const auto lines = Disassembly(words, s);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0].text == "LDA #$00");
    CHECK(lines[1].text == "BNE $1C00");
    CHECK(lines[1].address == 0x1C02);
    CHECK(lines[2].text == ".byte $FF");  // undocumented
    CHECK(lines[3].text == ".byte $20");  // JSR running past the end
    CHECK(lines[4].offset == 6);
    s.codeFrom = 2;
    s.codeLength = 3;
    const auto range = Disassembly(words, s);
    REQUIRE(range.size() == 2);
    CHECK(range[0].address == 0x1C02);
    CHECK(range[1].text == ".byte $FF");

    // The ZX80's interrupt routine, MASK-INT at $0038.
    const auto ic2 = Rom("bw80", "zx80_ic2.bin", 4096, 8);
    ContentsSettings z80;
    z80.cpu = Cpu::Z80;
    z80.codeFrom = 0x38;
    z80.codeLength = 4;
    const auto mask = Disassembly(ic2, z80);
    REQUIRE(mask.size() == 2);
    CHECK(mask[0].text == "DEC C");
    CHECK(mask[1].text == "JP NZ,$0045");
}

TEST_CASE("contents: glyphs from the character ROMs", "[contents]") {
    CHECK(GlyphLayout(434, 8).columns == 8);
    CHECK(GlyphLayout(434, 8).dot == 5);
    CHECK(GlyphLayout(800, 8).columns == 16);
    CHECK(GlyphLayout(100, 8).columns == 4);

    // The 2513's @, word 1: .###.
    const auto chars = Rom("message", "2513.rom", 512, 5);
    ContentsSettings c2513;
    c2513.glyphBits = 5;
    CHECK(GlyphCount(chars.size(), c2513) == 64);
    CHECK_FALSE(GlyphDot(chars, c2513, 0, 1, 0));
    CHECK(GlyphDot(chars, c2513, 0, 1, 1));
    CHECK(GlyphDot(chars, c2513, 0, 1, 3));
    CHECK_FALSE(GlyphDot(chars, c2513, 0, 1, 4));
    CHECK_FALSE(GlyphDot(chars, c2513, 64, 0, 0));  // past the end

    // The 901447-08's E (screen code 5), its third row the cell Odd E sticks: only the bar lit.
    const auto pet = Rom("bw2001", "901447-08.bin", 2048, 8);
    const ContentsSettings c8;
    CHECK(GlyphDot(pet, c8, 5, 2, 1));
    CHECK_FALSE(GlyphDot(pet, c8, 5, 2, 5));
    CHECK(GlyphDot(pet, c8, 5, 0, 6));  // the top bar

    // The ZX80's K at $0E00 + $30 x 8: rows 00 42 44 78 44 42 41 00.
    const auto ic2 = Rom("bw80", "zx80_ic2.bin", 4096, 8);
    ContentsSettings zx;
    zx.glyphFrom = 0xE00;
    CHECK(GlyphCount(ic2.size(), zx) == 64);
    CHECK(GlyphDot(ic2, zx, 0x30, 3, 1));  // 78: .####...
    CHECK(GlyphDot(ic2, zx, 0x30, 3, 4));
    CHECK_FALSE(GlyphDot(ic2, zx, 0x30, 3, 5));
    CHECK(GlyphDot(ic2, zx, 0x30, 6, 7));  // 41
    CHECK_FALSE(GlyphDot(ic2, zx, 0x30, 0, 1));
}
