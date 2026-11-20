#pragma once

// The binder's Contents page (P6p), ImGui-free: what each of its three views shows of a ROM chip's
// words (docs/p6p-memory-contents/MEMORY_CONTENTS_DESIGN.md §6).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "view/contents_defaults.hpp"

namespace bw {

enum class ContentsView : std::uint8_t { Hex, Code, Glyphs };

// One chip's display choices, kept for the run.
struct ContentsSettings {
    ContentsView view = ContentsView::Hex;
    std::uint16_t address = 0;  // where the CPU sees word 0: the hex and code views' numbering
    CharacterSet charset = CharacterSet::Ascii;
    Cpu cpu = Cpu::Mos6502;
    std::size_t codeFrom = 0, codeLength = 0;  // words; length 0: to the chip's end
    std::size_t glyphFrom = 0;                 // the first glyph's word
    unsigned glyphRows = 8, glyphBits = 8;     // words a glyph, low bits a row
    bool operator==(const ContentsSettings&) const = default;
};

// The first view and the settings a chip opens at: the Machine's CPU (else the 6502) and character
// set, the chip's address and glyphs (else 0), 8 rows of the word's width; Code when the chip has
// an address and the Machine a CPU, else Glyphs when it has glyphs, else Hex.
ContentsSettings DefaultSettings(const ContentsDefaults& defaults, std::string_view reference,
                                 unsigned bits);

std::string SizeText(std::size_t words, unsigned bits);  // "2K x 8", "512 x 5"

// The text column's character for `code`, UTF-8: one ASCII character, "£", or "." when the set
// has no letter for it (graphics, controls, tokens). A ZX80 inverse code reads as its character.
std::string_view CharacterOf(CharacterSet set, std::uint8_t code);

// Words in a hex row: 16, 8 or 4, the most whose row fits `characters` (a row of n takes 4n + 7).
int HexColumns(float characters);
// One hex row: "AAAA  WW WW …  text", the words from `from` (at most `columns`, a short last row
// padded so the text lines up), numbered from `address` + `from` on 16 bits.
std::string HexLine(std::span<const std::uint8_t> words, std::uint16_t address, std::size_t from,
                    int columns, CharacterSet set);

// One line of the code view.
struct CodeLine {
    std::uint16_t address;  // the CPU's: settings.address + offset
    std::size_t offset, length;
    std::string text;  // "LDA #$00", ".byte $FF"
};
// The settings' range disassembled in their CPU; an undocumented opcode, or an instruction running
// past the range, is one ".byte $XX" word.
std::vector<CodeLine> Disassembly(std::span<const std::uint8_t> words, const ContentsSettings& s);

// The glyph grid at a page `width` px: 16, 8 or 4 glyphs a row, the most whose dot is at least
// 3 px; a glyph takes bits + 2 dots across.
struct GlyphGrid {
    int columns;
    float dot;  // px
};
GlyphGrid GlyphLayout(float width, unsigned bits);
std::size_t GlyphCount(std::size_t words, const ContentsSettings& s);
// A lit dot: row `row` (a word) of glyph `glyph` from glyphFrom, column 0 the leftmost (bit
// glyphBits - 1). False past the chip's end.
bool GlyphDot(std::span<const std::uint8_t> words, const ContentsSettings& s, std::size_t glyph,
              unsigned row, unsigned column);

}  // namespace bw
