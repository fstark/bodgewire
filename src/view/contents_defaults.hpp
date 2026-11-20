#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace bw {

// The CPUs and character sets the binder's Contents page reads a ROM in (P6p).
enum class Cpu : std::uint8_t { Mos6502, Z80 };
enum class CharacterSet : std::uint8_t { Ascii, Petscii, Zx80 };

struct ChipDefaults {
    std::optional<std::uint16_t> address;  // where the CPU sees its first word
    std::optional<std::size_t> glyphs;     // the word where its character set starts
};

// A Machine's roms/contents.txt (P6p): what the Contents page opens each chip at, never more than
// defaults.
struct ContentsDefaults {
    std::optional<Cpu> cpu;
    CharacterSet charset = CharacterSet::Ascii;
    std::map<std::string, ChipDefaults, std::less<>> chips;
};

// Lines: `#` comments, `cpu 6502|z80`, `charset ascii|petscii|zx80` (each at most once),
// `chip REF [address N] [glyphs N]` (each chip once; N `$` hex or decimal). Errors:
// "<file> line N: <what>". The chips are not checked against a netlist.
std::expected<ContentsDefaults, std::string> ParseContentsDefaults(std::string_view text,
                                                                   std::string_view file);

}  // namespace bw
