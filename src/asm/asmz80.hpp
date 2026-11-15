#pragma once

#include "asm/instruction_set.hpp"

namespace bw::asm_detail {

// Zilog's documented Z80 instructions (UM0080), in Zilog's operand syntax
// (docs/p5t-z80-assembler/Z80_ASSEMBLER_DESIGN.md, Z80 instructions).
const InstructionSet& Z80Instructions();

}  // namespace bw::asm_detail
