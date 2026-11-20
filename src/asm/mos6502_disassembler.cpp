#include "asm/mos6502_disassembler.hpp"

#include <format>
#include <string>

#include "parts/mos6502_core.hpp"

namespace bw {

std::optional<Disassembled> Disassemble6502(std::span<const std::uint8_t> bytes, std::uint16_t at) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    const Mos6502Instruction i = Mos6502Decode(bytes[0]);
    const std::size_t operands = Mos6502OperandBytes(i.operand);
    if (!i.documented || bytes.size() < 1 + operands) {
        return std::nullopt;
    }
    const unsigned n = operands >= 1 ? bytes[1] : 0;
    const unsigned nn = operands == 2 ? n | (unsigned{bytes[2]} << 8) : n;
    const std::string m(i.mnemonic);
    std::string text;
    using enum Mos6502Operand;
    switch (i.operand) {
    case None:
        text = m;
        break;
    case Accumulator:
        text = m + " A";
        break;
    case Immediate:
        text = std::format("{} #${:02X}", m, n);
        break;
    case ZeroPage:
        text = std::format("{} ${:02X}", m, n);
        break;
    case ZeroPageX:
        text = std::format("{} ${:02X},X", m, n);
        break;
    case ZeroPageY:
        text = std::format("{} ${:02X},Y", m, n);
        break;
    case Absolute:
        text = std::format("{} ${:04X}", m, nn);
        break;
    case AbsoluteX:
        text = std::format("{} ${:04X},X", m, nn);
        break;
    case AbsoluteY:
        text = std::format("{} ${:04X},Y", m, nn);
        break;
    case Indirect:
        text = std::format("{} (${:04X})", m, nn);
        break;
    case IndirectX:
        text = std::format("{} (${:02X},X)", m, n);
        break;
    case IndirectY:
        text = std::format("{} (${:02X}),Y", m, n);
        break;
    case Relative: {
        const int target = at + 2 + static_cast<std::int8_t>(n);
        text = std::format("{} ${:04X}", m, static_cast<unsigned>(target) & 0xFFFF);
        break;
    }
    }
    return Disassembled{std::move(text), 1 + operands};
}

}  // namespace bw
