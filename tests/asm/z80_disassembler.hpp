#pragma once

// A test-only Z80 disassembler for Zilog's documented instructions, decoded by opcode fields
// x/y/z/p/q (Cristian Dinu, "Decoding Z80 Opcodes"), written apart from the assembler's form
// table so the two check each other (docs/p5t-z80-assembler/Z80_ASSEMBLER_DESIGN.md, Tests).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bw {

struct Z80Text {
    std::string text;    // Zilog syntax, upper case, numbers as $hex: "LD B,(IX-3)"
    std::size_t length;  // bytes taken
};

// The instruction at the start of `bytes`, at address `at` (for relative targets); nullopt for an
// undocumented opcode or too few bytes. ED 63 and ED 6B (LD (nn),HL and LD HL,(nn) by the ED
// table) count as undocumented: every assembler gives the 3-byte 22 and 2A.
std::optional<Z80Text> DisassembleZ80(std::span<const std::uint8_t> bytes, std::uint16_t at);

// Every documented opcode with its operand bytes: n $34 and nn $1234 unprefixed and after ED,
// relative offset $34; after DD or FD, d $FD (-3) and n $5A, or nn $5AFD. 696 sequences.
std::vector<std::vector<std::uint8_t>> Z80DocumentedOpcodes();

}  // namespace bw
