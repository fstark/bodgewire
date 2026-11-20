#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "asm/disassembled.hpp"

namespace bw {

// The documented 6502 instruction at the start of `bytes`, at address `at` (for branch targets)
// (P6p): MOS syntax, upper case, $hex ("LDA ($FA),Y", "BNE $1C4F", "ASL A"); an absolute operand
// always 4 digits ("LDA $0034": real code uses absolute on zero page). nullopt for an
// undocumented opcode or too few bytes. Decoded from the core's own table (Mos6502Decode).
std::optional<Disassembled> Disassemble6502(std::span<const std::uint8_t> bytes, std::uint16_t at);

}  // namespace bw
