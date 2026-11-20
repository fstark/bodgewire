#pragma once

#include <cstdint>
#include <vector>

namespace bw {

// Every documented Z80 opcode with its operand bytes, as DisassembleZ80 accepts them: n $34 and
// nn $1234 unprefixed and after ED, relative offset $34; after DD or FD, d $FD (-3) and n $5A, or
// nn $5AFD. 696 sequences.
std::vector<std::vector<std::uint8_t>> Z80DocumentedOpcodes();

}  // namespace bw
