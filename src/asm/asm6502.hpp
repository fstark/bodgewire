#pragma once

#include "asm/instruction_set.hpp"

namespace bw::asm_detail {

// The 151 documented NMOS 6502 opcodes, encoded from the core's own table (Mos6502Decode).
const InstructionSet& Mos6502Instructions();

}  // namespace bw::asm_detail
