#pragma once

// A Z80 disassembler for Zilog's documented instructions, decoded by opcode fields x/y/z/p/q
// (Cristian Dinu, "Decoding Z80 Opcodes"), written apart from the assembler's form table so the
// two check each other (docs/p5t-z80-assembler/Z80_ASSEMBLER_DESIGN.md, Tests). The tests' first,
// the binder's Contents page's since P6p.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "asm/disassembled.hpp"

namespace bw {

// The instruction at the start of `bytes`, at address `at` (for relative targets); nullopt for an
// undocumented opcode or too few bytes. ED 63 and ED 6B (LD (nn),HL and LD HL,(nn) by the ED
// table) count as undocumented: every assembler gives the 3-byte 22 and 2A.
std::optional<Disassembled> DisassembleZ80(std::span<const std::uint8_t> bytes, std::uint16_t at);

}  // namespace bw
