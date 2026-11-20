#include "view/memory_contents.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>

#include "asm/mos6502_disassembler.hpp"
#include "asm/z80_disassembler.hpp"

namespace bw {

namespace {

// Every code 0-127 as a one-character string.
constexpr std::array<char, 128> kAscii = [] {
    std::array<char, 128> chars{};
    for (std::size_t i = 0; i < chars.size(); ++i) {
        chars[i] = static_cast<char>(i);
    }
    return chars;
}();

std::string_view Plain(unsigned c) { return {&kAscii[c], 1}; }

constexpr std::string_view kDot = ".";
constexpr std::string_view kPound = "\xC2\xA3";  // £

// The ZX80's characters $10-$1B.
constexpr std::string_view kZx80Signs = "()-+*/=><;,.";

}  // namespace

ContentsSettings DefaultSettings(const ContentsDefaults& defaults, std::string_view reference) {
    ContentsSettings s;
    s.charset = defaults.charset;
    s.cpu = defaults.cpu.value_or(Cpu::Mos6502);
    const auto chip = defaults.chips.find(reference);
    const ChipDefaults found = chip == defaults.chips.end() ? ChipDefaults{} : chip->second;
    s.address = found.address.value_or(0);
    s.glyphFrom = found.glyphs.value_or(0);
    if (found.address && defaults.cpu) {
        s.view = ContentsView::Code;
    } else if (found.glyphs) {
        s.view = ContentsView::Glyphs;
    }
    return s;
}

std::string SizeText(std::size_t words, unsigned bits) {
    if (words >= 1024 && words % 1024 == 0) {
        return std::format("{}K x {}", words / 1024, bits);
    }
    return std::format("{} x {}", words, bits);
}

std::string_view CharacterOf(CharacterSet set, std::uint8_t code) {
    switch (set) {
    case CharacterSet::Ascii:
        return code >= 0x20 && code <= 0x7E ? Plain(code) : kDot;
    case CharacterSet::Petscii:
        if (code == 0x5C) {
            return kPound;
        }
        return code >= 0x20 && code <= 0x5D ? Plain(code) : kDot;
    case CharacterSet::Zx80: {
        const unsigned c = code & 0x7Fu;  // bit 7: inverse, the same character
        if (c == 0x00) {
            return " ";
        }
        if (c == 0x01) {
            return "\"";
        }
        if (c == 0x0C) {
            return kPound;
        }
        if (c >= 0x0D && c <= 0x0F) {
            return Plain("$:?"[c - 0x0D]);
        }
        if (c >= 0x10 && c <= 0x1B) {
            return kZx80Signs.substr(c - 0x10, 1);
        }
        if (c >= 0x1C && c <= 0x25) {
            return Plain('0' + (c - 0x1C));
        }
        if (c >= 0x26 && c <= 0x3F) {
            return Plain('A' + (c - 0x26));
        }
        return kDot;  // the block graphics $02-$0B, the tokens $40-$7F
    }
    }
    return kDot;
}

int HexColumns(float characters) {
    for (const int columns : {16, 8}) {
        if (characters >= static_cast<float>(4 * columns + 7)) {
            return columns;
        }
    }
    return 4;
}

std::string HexLine(std::span<const std::uint8_t> words, std::uint16_t address, std::size_t from,
                    int columns, CharacterSet set) {
    std::string line = std::format("{:04X}  ", (address + from) & 0xFFFFu);
    std::string text;
    for (std::size_t i = 0; i < static_cast<std::size_t>(columns); ++i) {
        if (i > 0) {
            line += ' ';
        }
        if (from + i < words.size()) {
            line += std::format("{:02X}", words[from + i]);
            text += CharacterOf(set, words[from + i]);
        } else {
            line += "  ";
        }
    }
    return line + "  " + text;
}

std::vector<CodeLine> Disassembly(std::span<const std::uint8_t> words, const ContentsSettings& s) {
    const std::size_t from = std::min(s.codeFrom, words.size());
    const std::size_t end =
        s.codeLength == 0 ? words.size() : std::min(words.size(), from + s.codeLength);
    std::vector<CodeLine> lines;
    for (std::size_t offset = from; offset < end;) {
        const auto at = static_cast<std::uint16_t>(s.address + offset);
        const auto bytes = words.subspan(offset, end - offset);
        const auto one =
            s.cpu == Cpu::Mos6502 ? Disassemble6502(bytes, at) : DisassembleZ80(bytes, at);
        if (one) {
            lines.push_back({at, offset, one->length, one->text});
        } else {
            lines.push_back({at, offset, 1, std::format(".byte ${:02X}", words[offset])});
        }
        offset += lines.back().length;
    }
    return lines;
}

GlyphGrid GlyphLayout(float width) {
    constexpr auto across = static_cast<float>(kGlyphDots + 2);
    for (const int columns : {16, 8}) {
        const float dot = std::floor(width / (static_cast<float>(columns) * across));
        if (dot >= 3) {
            return {columns, dot};
        }
    }
    return {4, std::max(1.0f, std::floor(width / (4 * across)))};
}

std::size_t GlyphCount(std::size_t words, const ContentsSettings& s) {
    if (s.glyphFrom >= words || s.glyphRows == 0) {
        return 0;
    }
    return (words - s.glyphFrom + s.glyphRows - 1) / s.glyphRows;
}

bool GlyphDot(std::span<const std::uint8_t> words, const ContentsSettings& s, std::size_t glyph,
              unsigned row, unsigned column) {
    const std::size_t word = s.glyphFrom + glyph * s.glyphRows + row;
    if (word >= words.size() || row >= s.glyphRows || column >= kGlyphDots) {
        return false;
    }
    return ((words[word] >> (kGlyphDots - 1 - column)) & 1u) != 0;
}

}  // namespace bw
