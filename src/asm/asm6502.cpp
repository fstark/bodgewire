#include "asm/asm6502.hpp"

#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "parts/mos6502_core.hpp"

namespace bw::asm_detail {

namespace {

using enum Mos6502Operand;

std::string_view FormName(Mos6502Operand form) {
    switch (form) {
    case None:
        return "implied";
    case Accumulator:
        return "A";
    case Immediate:
        return "#";
    case ZeroPage:
        return "zp";
    case ZeroPageX:
        return "zp,X";
    case ZeroPageY:
        return "zp,Y";
    case Absolute:
        return "abs";
    case AbsoluteX:
        return "abs,X";
    case AbsoluteY:
        return "abs,Y";
    case Indirect:
        return "(abs)";
    case IndirectX:
        return "(zp,X)";
    case IndirectY:
        return "(zp),Y";
    case Relative:
        return "relative";
    }
    return "?";
}

std::size_t OperandBytes(Mos6502Operand form) {
    switch (form) {
    case None:
    case Accumulator:
        return 0;
    case Absolute:
    case AbsoluteX:
    case AbsoluteY:
    case Indirect:
        return 2;
    default:
        return 1;
    }
}

// (mnemonic, form) → opcode, documented opcodes only.
class Encodings {
public:
    Encodings() {
        for (int code = 0; code < 256; ++code) {
            const Mos6502Instruction i = Mos6502Decode(static_cast<std::uint8_t>(code));
            if (i.documented) {
                table_.emplace(Key(i.mnemonic, i.operand), static_cast<std::uint8_t>(code));
            }
        }
    }
    std::optional<std::uint8_t> find(std::string_view mnemonic, Mos6502Operand form) const {
        const auto it = table_.find(Key(mnemonic, form));
        if (it == table_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    bool knows(std::string_view mnemonic) const {
        const auto it = table_.lower_bound(Key(mnemonic, None));
        return it != table_.end() && it->first.first == mnemonic;
    }

private:
    using KeyType = std::pair<std::string, int>;
    static KeyType Key(std::string_view mnemonic, Mos6502Operand form) {
        return {std::string(mnemonic), static_cast<int>(form)};
    }
    std::map<KeyType, std::uint8_t> table_;
};

// The operand's syntax (ZeroPage standing for plain, ZeroPageX/Y for indexed) and its
// expression text.
std::pair<Mos6502Operand, std::string_view> Split(std::string_view text) {
    text = Trim(text);
    const std::string s = Squeeze(text);
    if (text.starts_with('#')) {
        return {Immediate, text.substr(1)};
    }
    if (text.starts_with('(')) {
        const std::size_t close = text.rfind(')');
        if (close == std::string_view::npos) {
            Fail("missing )");
        }
        if (s.ends_with(",X)")) {
            const std::size_t comma = text.rfind(',');
            return {IndirectX, text.substr(1, comma - 1)};
        }
        if (s.ends_with("),Y")) {
            return {IndirectY, text.substr(1, close - 1)};
        }
        if (s.ends_with(")")) {
            return {Indirect, text.substr(1, close - 1)};
        }
        Fail(std::format("bad operand '{}'", text));
    }
    if (s.ends_with(",X") || s.ends_with(",Y")) {
        const std::size_t comma = text.rfind(',');
        return {s.back() == 'X' ? ZeroPageX : ZeroPageY, text.substr(0, comma)};
    }
    return {ZeroPage, text};
}

class Mos6502Set final : public InstructionSet {
public:
    bool knows(std::string_view mnemonic) const override { return encodings_.knows(mnemonic); }

    void assemble(AsmContext& context, std::string_view mnemonic,
                  std::string_view operandText) const override {
        Mos6502Operand form = None;
        AsmValue value{};
        if (context.pass() == 1) {
            const auto [chosen, v] = chooseForm(context, mnemonic, operandText);
            form = chosen;
            value = v;
            context.memo() = static_cast<int>(form);
        } else {
            form = static_cast<Mos6502Operand>(context.memo());
            if (form != None && form != Accumulator) {
                value = context.expression(Split(operandText).second);
            }
        }
        const std::optional<std::uint8_t> opcode = encodings_.find(mnemonic, form);
        if (!opcode) {
            Fail(std::format("{} has no {} form", mnemonic, FormName(form)));
        }
        const std::int64_t at = context.pc();
        context.emit(*opcode);
        switch (OperandBytes(form)) {
        case 0:
            break;
        case 1: {
            std::int64_t byte = value.number;
            if (form == Relative) {
                context.check(value, 0, 0xFFFF, "branch target");
                byte = value.number - (at + 2);
                if (context.pass() == 2 && (byte < -128 || byte > 127)) {
                    Fail(std::format("branch out of range ({} bytes)", byte));
                }
            } else if (form == Immediate) {
                context.check(value, -128, 255, "immediate");
            } else {
                context.check(value, 0, 255, FormName(form));
            }
            context.emit(static_cast<std::uint8_t>(byte & 0xFF));
            break;
        }
        default:
            context.check(value, 0, 0xFFFF, FormName(form));
            context.emit(static_cast<std::uint8_t>(value.number & 0xFF));
            context.emit(static_cast<std::uint8_t>((value.number >> 8) & 0xFF));
        }
    }

private:
    // Pass 1: the form from the operand's syntax and, between zero page and absolute, its value.
    std::pair<Mos6502Operand, AsmValue> chooseForm(AsmContext& context, std::string_view mnemonic,
                                                   std::string_view text) const {
        const auto has = [&](Mos6502Operand form) { return encodings_.find(mnemonic, form); };
        const std::string s = Squeeze(text);
        if (s.empty()) {
            return {has(None) || !has(Accumulator) ? None : Accumulator, {}};
        }
        if (s == "A") {
            return {Accumulator, {}};
        }
        const auto [syntax, expr] = Split(text);
        const AsmValue v = context.expression(expr);
        const bool small = v.known && v.number >= 0 && v.number <= 0xFF;
        const auto pick = [&](Mos6502Operand zp, Mos6502Operand abs) {
            if (small && has(zp)) {
                return zp;
            }
            if (!has(abs) && has(zp) && !v.known) {
                return zp;  // a forward reference: checked in pass 2
            }
            return abs;
        };
        switch (syntax) {
        case ZeroPage:  // plain
            if (has(Relative)) {
                return {Relative, v};
            }
            return {pick(ZeroPage, Absolute), v};
        case ZeroPageX:
            return {pick(ZeroPageX, AbsoluteX), v};
        case ZeroPageY:
            return {pick(ZeroPageY, AbsoluteY), v};
        default:
            return {syntax, v};
        }
    }

    Encodings encodings_;
};

}  // namespace

const InstructionSet& Mos6502Instructions() {
    static const Mos6502Set set;
    return set;
}

}  // namespace bw::asm_detail
