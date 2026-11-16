#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

// A ROM image assembled from a 6502 or Z80 program.
struct AssembledRom {
    std::uint16_t base = 0;           // CPU address of the ROM's word 0 (.rom)
    std::vector<std::uint8_t> words;  // the whole ROM; unwritten words 0xFF (erased)
    std::string text;                 // the .rom file (docs/p4l-rom-parts/ROM_PARTS.md format)
};

// Two-pass assembler for the documented opcodes of the NMOS 6502 (151) and the Z80
// (docs/p5d-rom-programs/ROM_PROGRAMS.md, What is new 1; docs/p5t-z80-assembler/Z80_ASSEMBLER.md).
// Common to both:
//   label:  @local:  NAME = expr  ; comment
//   .cpu 6502|z80 (once, before any instruction; the 6502 without it)
//   .rom BASE, SIZE (first, once)  .org expr  .byte expr|"text", ...  .word expr, ...
//   .checksum: one byte making the whole ROM (unwritten 0xFF words too) sum to 0 mod 256, once;
//   .checksum BASE, SIZE: the same for that window (one a chip), windows not overlapping
//   .include "file": the file's lines here, resolved against the including file's directory
//   .incbin "file", OFFSET, LENGTH: LENGTH bytes of a binary file from OFFSET (resolved so too)
//   expressions: $hex %bin decimal 'c' symbol * (this statement's address), unary - < > ~,
//   binary * / + - << >> & ^ | with C's precedence, parentheses.
// 6502 operands: (none) A #e e e,X e,Y (e) (e,X) (e),Y; an operand starting with ( is indirect;
//   zero page when e is known in the first pass and 0-255, else absolute.
// Z80 operands, Zilog's (UM0080): A B C D E H L I R, AF BC DE HL SP IX IY AF', (HL) (BC) (DE)
//   (SP) (C), (IX+e) (IX-e) (IX) and IY's, conditions NZ Z NC C PO PE P M first in JP CALL RET
//   (NZ Z NC C in JR), e: data, a target or a bit; an operand wholly inside one pair of
//   parentheses is memory or a port: LD A,(BUF+1) reads memory, LD A,(1+2)*3 loads 9.
//   ADD/ADC/SBC take A, first; SUB AND XOR OR CP take one operand. A symbol named like a
//   register or condition reads as that register in an operand.
// `file` names the source in errors ("<file> line N: <what>") and in the .rom's header.
// `includeDir` resolves `.include` and `.incbin`; without it they are errors (no file on disk).
std::expected<AssembledRom, std::string>
Assemble(std::string_view source, std::string_view file,
         std::optional<std::filesystem::path> includeDir = std::nullopt);

// The same from a file; "cannot read '<path>'"; errors name the file's filename.
std::expected<AssembledRom, std::string> AssembleFile(const std::filesystem::path& path);

}  // namespace bw
