#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

// A ROM image assembled from a 6502 program.
struct AssembledRom {
    std::uint16_t base = 0;           // CPU address of the ROM's word 0 (.rom)
    std::vector<std::uint8_t> words;  // the whole ROM; unwritten words 0xFF (erased)
    std::string text;                 // the .rom file (docs/p4l-rom-parts/ROM_PARTS.md format)
};

// Two-pass assembler for the 151 documented NMOS 6502 opcodes
// (docs/p5d-rom-programs/ROM_PROGRAMS.md, What is new 1). In short:
//   label:  @local:  NAME = expr  ; comment
//   .rom BASE, SIZE (first, once)  .org expr  .byte expr|"text", ...  .word expr, ...
//   .checksum: one byte making the whole ROM (unwritten 0xFF words too) sum to 0 mod 256, once;
//   .checksum BASE, SIZE: the same for that window (one a chip), windows not overlapping
//   .include "file": the file's lines here, resolved against the including file's directory
//   operands: (none) A #e e e,X e,Y (e) (e,X) (e),Y; an operand starting with ( is indirect;
//   zero page when e is known in the first pass and 0-255, else absolute.
//   expressions: $hex %bin decimal 'c' symbol * (this statement's address), unary - < > ~,
//   binary * / + - << >> & ^ | with C's precedence, parentheses.
// `file` names the source in errors ("<file> line N: <what>") and in the .rom's header.
// `includeDir` resolves `.include`; without it `.include` is an error (no file on disk).
std::expected<AssembledRom, std::string>
Assemble6502(std::string_view source, std::string_view file,
             std::optional<std::filesystem::path> includeDir = std::nullopt);

// The same from a file; "cannot read '<path>'"; errors name the file's filename.
std::expected<AssembledRom, std::string> Assemble6502File(const std::filesystem::path& path);

}  // namespace bw
