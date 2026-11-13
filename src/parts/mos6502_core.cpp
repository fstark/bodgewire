#include "parts/mos6502_core.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace bw {

namespace {

constexpr std::uint8_t kC = 0x01, kZ = 0x02, kI = 0x04, kD = 0x08, kB = 0x10, kU = 0x20, kV = 0x40,
                       kN = 0x80;
constexpr std::uint16_t kNmiVector = 0xFFFA, kResetVector = 0xFFFC, kIrqVector = 0xFFFE;

enum class Mode : std::uint8_t {
    Implied,
    Accumulator,
    Immediate,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    IndirectX,
    IndirectY,
    Relative,
    JumpAbsolute,
    JumpIndirect,
    Call,
    Return,
    ReturnFromInterrupt,
    Break,
    Push,
    Pull,
    Jam
};

enum class Access : std::uint8_t { None, Read, Write, Modify };

enum class Op : std::uint8_t {
    // Read
    Lda,
    Ldx,
    Ldy,
    Lax,
    Adc,
    Sbc,
    And,
    Ora,
    Eor,
    Cmp,
    Cpx,
    Cpy,
    Bit,
    Nop,
    Las,
    Anc,
    Alr,
    Arr,
    Sbx,
    Ane,
    Lxa,
    // Write
    Sta,
    Stx,
    Sty,
    Sax,
    Sha,
    Shx,
    Shy,
    Tas,
    // Modify (memory or the accumulator)
    Asl,
    Lsr,
    Rol,
    Ror,
    Inc,
    Dec,
    Slo,
    Rla,
    Sre,
    Rra,
    Dcp,
    Isc,
    // Implied
    Clc,
    Sec,
    Cli,
    Sei,
    Clv,
    Cld,
    Sed,
    Tax,
    Txa,
    Tay,
    Tya,
    Tsx,
    Txs,
    Inx,
    Iny,
    Dex,
    Dey,
    // Stack, branches, jumps
    Pha,
    Php,
    Pla,
    Plp,
    Bpl,
    Bmi,
    Bvc,
    Bvs,
    Bcc,
    Bcs,
    Bne,
    Beq,
    Jmp,
    Jsr,
    Rts,
    Rti,
    Brk,
    Jam
};

struct Opcode {
    Mode mode = Mode::Jam;
    Access access = Access::None;
    Op op = Op::Jam;
};

constexpr Access AccessOf(Op op) {
    if (op <= Op::Lxa) {
        return Access::Read;
    }
    if (op <= Op::Tas) {
        return Access::Write;
    }
    if (op <= Op::Isc) {
        return Access::Modify;
    }
    return Access::None;
}

// The NMOS opcode matrix, documented and undocumented.
constexpr std::array<Opcode, 256> BuildOpcodes() {
    std::array<Opcode, 256> t{};
    const auto set = [&t](int code, Mode mode, Op op) {
        t[static_cast<std::size_t>(code)] = {mode, AccessOf(op), op};
    };
    using enum Mode;
    // cc = 01 and cc = 11: aaa selects the operation, bbb the addressing.
    constexpr std::array<Op, 8> kGroup1{Op::Ora, Op::And, Op::Eor, Op::Adc,
                                        Op::Sta, Op::Lda, Op::Cmp, Op::Sbc};
    constexpr std::array<Op, 8> kGroup3{Op::Slo, Op::Rla, Op::Sre, Op::Rra,
                                        Op::Sax, Op::Lax, Op::Dcp, Op::Isc};
    constexpr std::array<Mode, 8> kModes1{IndirectX, ZeroPage,  Immediate, Absolute,
                                          IndirectY, ZeroPageX, AbsoluteY, AbsoluteX};
    for (int aaa = 0; aaa < 8; ++aaa) {
        for (int bbb = 0; bbb < 8; ++bbb) {
            const int code = aaa << 5 | bbb << 2;
            set(code | 1, kModes1[static_cast<std::size_t>(bbb)],
                kGroup1[static_cast<std::size_t>(aaa)]);
            set(code | 3, kModes1[static_cast<std::size_t>(bbb)],
                kGroup3[static_cast<std::size_t>(aaa)]);
        }
    }
    set(0x89, Immediate, Op::Nop);
    set(0x97, ZeroPageY, Op::Sax);
    set(0xB7, ZeroPageY, Op::Lax);
    set(0xBF, AbsoluteY, Op::Lax);
    set(0x0B, Immediate, Op::Anc);
    set(0x2B, Immediate, Op::Anc);
    set(0x4B, Immediate, Op::Alr);
    set(0x6B, Immediate, Op::Arr);
    set(0x8B, Immediate, Op::Ane);
    set(0xAB, Immediate, Op::Lxa);
    set(0xCB, Immediate, Op::Sbx);
    set(0xEB, Immediate, Op::Sbc);
    set(0x93, IndirectY, Op::Sha);
    set(0x9F, AbsoluteY, Op::Sha);
    set(0x9B, AbsoluteY, Op::Tas);
    set(0xBB, AbsoluteY, Op::Las);

    // cc = 10: shifts, increments, X loads and stores.
    constexpr std::array<Op, 8> kGroup2{Op::Asl, Op::Rol, Op::Lsr, Op::Ror,
                                        Op::Stx, Op::Ldx, Op::Dec, Op::Inc};
    for (int aaa = 0; aaa < 8; ++aaa) {
        const Op op = kGroup2[static_cast<std::size_t>(aaa)];
        const int code = aaa << 5 | 2;
        const bool x = op == Op::Stx || op == Op::Ldx;
        set(code | 0 << 2, Jam, Op::Jam);
        set(code | 1 << 2, ZeroPage, op);
        set(code | 2 << 2, Accumulator, op);
        set(code | 3 << 2, Absolute, op);
        set(code | 4 << 2, Jam, Op::Jam);
        set(code | 5 << 2, x ? ZeroPageY : ZeroPageX, op);
        set(code | 6 << 2, Implied, Op::Nop);
        set(code | 7 << 2, x ? AbsoluteY : AbsoluteX, op);
    }
    set(0x82, Immediate, Op::Nop);
    set(0xA2, Immediate, Op::Ldx);
    set(0xC2, Immediate, Op::Nop);
    set(0xE2, Immediate, Op::Nop);
    set(0x8A, Implied, Op::Txa);
    set(0xAA, Implied, Op::Tax);
    set(0xCA, Implied, Op::Dex);
    set(0xEA, Implied, Op::Nop);
    set(0x9A, Implied, Op::Txs);
    set(0xBA, Implied, Op::Tsx);
    set(0x9E, AbsoluteY, Op::Shx);

    // cc = 00.
    constexpr std::array<Op, 8> kBranches{Op::Bpl, Op::Bmi, Op::Bvc, Op::Bvs,
                                          Op::Bcc, Op::Bcs, Op::Bne, Op::Beq};
    constexpr std::array<Op, 8> kFlags{Op::Clc, Op::Sec, Op::Cli, Op::Sei,
                                       Op::Tya, Op::Clv, Op::Cld, Op::Sed};
    for (int aaa = 0; aaa < 8; ++aaa) {
        const int code = aaa << 5;
        set(code | 0x04, ZeroPage, Op::Nop);
        set(code | 0x0C, Absolute, Op::Nop);
        set(code | 0x10, Relative, kBranches[static_cast<std::size_t>(aaa)]);
        set(code | 0x14, ZeroPageX, Op::Nop);
        set(code | 0x18, Implied, kFlags[static_cast<std::size_t>(aaa)]);
        set(code | 0x1C, AbsoluteX, Op::Nop);
    }
    set(0x00, Break, Op::Brk);
    set(0x20, Call, Op::Jsr);
    set(0x40, ReturnFromInterrupt, Op::Rti);
    set(0x60, Return, Op::Rts);
    set(0x80, Immediate, Op::Nop);
    set(0xA0, Immediate, Op::Ldy);
    set(0xC0, Immediate, Op::Cpy);
    set(0xE0, Immediate, Op::Cpx);
    set(0x24, ZeroPage, Op::Bit);
    set(0x84, ZeroPage, Op::Sty);
    set(0xA4, ZeroPage, Op::Ldy);
    set(0xC4, ZeroPage, Op::Cpy);
    set(0xE4, ZeroPage, Op::Cpx);
    set(0x08, Push, Op::Php);
    set(0x28, Pull, Op::Plp);
    set(0x48, Push, Op::Pha);
    set(0x68, Pull, Op::Pla);
    set(0x88, Implied, Op::Dey);
    set(0xA8, Implied, Op::Tay);
    set(0xC8, Implied, Op::Iny);
    set(0xE8, Implied, Op::Inx);
    set(0x2C, Absolute, Op::Bit);
    set(0x4C, JumpAbsolute, Op::Jmp);
    set(0x6C, JumpIndirect, Op::Jmp);
    set(0x8C, Absolute, Op::Sty);
    set(0xAC, Absolute, Op::Ldy);
    set(0xCC, Absolute, Op::Cpy);
    set(0xEC, Absolute, Op::Cpx);
    set(0x94, ZeroPageX, Op::Sty);
    set(0xB4, ZeroPageX, Op::Ldy);
    set(0x9C, AbsoluteX, Op::Shy);
    set(0xBC, AbsoluteX, Op::Ldy);
    return t;
}

constexpr std::array<Opcode, 256> kOpcodes = BuildOpcodes();

// Mnemonics in the order of Op.
constexpr std::array<std::string_view, static_cast<std::size_t>(Op::Jam) + 1> kMnemonics{
    "LDA", "LDX", "LDY", "LAX", "ADC", "SBC", "AND", "ORA", "EOR", "CMP", "CPX", "CPY", "BIT",
    "NOP", "LAS", "ANC", "ALR", "ARR", "SBX", "ANE", "LXA", "STA", "STX", "STY", "SAX", "SHA",
    "SHX", "SHY", "TAS", "ASL", "LSR", "ROL", "ROR", "INC", "DEC", "SLO", "RLA", "SRE", "RRA",
    "DCP", "ISC", "CLC", "SEC", "CLI", "SEI", "CLV", "CLD", "SED", "TAX", "TXA", "TAY", "TYA",
    "TSX", "TXS", "INX", "INY", "DEX", "DEY", "PHA", "PHP", "PLA", "PLP", "BPL", "BMI", "BVC",
    "BVS", "BCC", "BCS", "BNE", "BEQ", "JMP", "JSR", "RTS", "RTI", "BRK", "JAM"};
static_assert(kMnemonics[static_cast<std::size_t>(Op::Lda)] == "LDA" &&
              kMnemonics[static_cast<std::size_t>(Op::Dey)] == "DEY" && kMnemonics.back() == "JAM");

constexpr bool IsDocumentedOp(Op op) {
    switch (op) {
    case Op::Lax:
    case Op::Las:
    case Op::Anc:
    case Op::Alr:
    case Op::Arr:
    case Op::Sbx:
    case Op::Ane:
    case Op::Lxa:
    case Op::Sax:
    case Op::Sha:
    case Op::Shx:
    case Op::Shy:
    case Op::Tas:
    case Op::Slo:
    case Op::Rla:
    case Op::Sre:
    case Op::Rra:
    case Op::Dcp:
    case Op::Isc:
    case Op::Jam:
        return false;
    default:
        return true;
    }
}

constexpr Mos6502Operand OperandOf(Mode mode) {
    switch (mode) {
    case Mode::Accumulator:
        return Mos6502Operand::Accumulator;
    case Mode::Immediate:
        return Mos6502Operand::Immediate;
    case Mode::ZeroPage:
        return Mos6502Operand::ZeroPage;
    case Mode::ZeroPageX:
        return Mos6502Operand::ZeroPageX;
    case Mode::ZeroPageY:
        return Mos6502Operand::ZeroPageY;
    case Mode::Absolute:
    case Mode::JumpAbsolute:
    case Mode::Call:
        return Mos6502Operand::Absolute;
    case Mode::AbsoluteX:
        return Mos6502Operand::AbsoluteX;
    case Mode::AbsoluteY:
        return Mos6502Operand::AbsoluteY;
    case Mode::JumpIndirect:
        return Mos6502Operand::Indirect;
    case Mode::IndirectX:
        return Mos6502Operand::IndirectX;
    case Mode::IndirectY:
        return Mos6502Operand::IndirectY;
    case Mode::Relative:
        return Mos6502Operand::Relative;
    default:  // Implied, Return, ReturnFromInterrupt, Break, Push, Pull, Jam
        return Mos6502Operand::None;
    }
}

constexpr std::uint16_t Stack(std::uint8_t s) { return static_cast<std::uint16_t>(0x0100 | s); }

constexpr std::uint16_t Word(std::uint8_t high, std::uint8_t low) {
    return static_cast<std::uint16_t>(high << 8 | low);
}

constexpr std::uint8_t Low(std::uint16_t word) { return static_cast<std::uint8_t>(word & 0xFF); }
constexpr std::uint8_t High(std::uint16_t word) { return static_cast<std::uint8_t>(word >> 8); }

}  // namespace

void Mos6502Core::start(const Mos6502Registers& registers) {
    r_ = registers;
    t_ = 1;
    stage_ = Stage::Addressing;
    jammed_ = false;
    resetHeld_ = false;
    interruptNext_ = false;
    nmiLatched_ = false;
    pollLast_ = false;
    pollFetch_ = false;
    bus_ = {r_.pc, 0, true, true};
}

void Mos6502Core::setNmi(bool asserted) {
    if (asserted && !nmiLevel_) {
        nmiLatched_ = true;
    }
    nmiLevel_ = asserted;
}

void Mos6502Core::setOverflow() { r_.p |= kV; }

void Mos6502Core::read(std::uint16_t address) { bus_ = {address, 0, true, false}; }

void Mos6502Core::write(std::uint16_t address, std::uint8_t data) {
    bus_ = {address, data, false, false};
}

void Mos6502Core::step(std::uint8_t data) {
    if (reset_) {
        // Held in reset: reads at PC; the instruction is abandoned.
        resetHeld_ = true;
        jammed_ = false;
        nmiLatched_ = false;
        interruptNext_ = false;
        stage_ = Stage::Addressing;
        read(r_.pc);
        return;
    }
    const bool wanted = pollLast_;  // at the end of the cycle before this one
    bool fetch = false;
    if (resetHeld_) {
        // The cycle just ended counts as the reset sequence's T1.
        resetHeld_ = false;
        opcode_ = 0x00;
        breakKind_ = BreakKind::Reset;
        t_ = 2;
        read(r_.pc);
    } else if (jammed_) {
        read(jamCycle_ == 1 || jamCycle_ == 2 ? 0xFFFE : 0xFFFF);
        jamCycle_ = jamCycle_ < 3 ? jamCycle_ + 1 : 3;
        return;
    } else if (t_ == 1) {
        if (interruptNext_) {
            interruptNext_ = false;
            opcode_ = 0x00;
            breakKind_ = nmiLatched_ ? BreakKind::Nmi : BreakKind::Irq;
        } else {
            opcode_ = data;
            breakKind_ = BreakKind::Break;
            ++r_.pc;
        }
        t_ = 2;
        stage_ = Stage::Addressing;
        fetch = true;
        read(r_.pc);
    } else {
        switch (stage_) {
        case Stage::Addressing:
            addressing(data);
            break;
        case Stage::Access:
            endAccess(data);
            break;
        case Stage::ModifyOld:
            value_ = modifyOperation(value_);
            write(address_, value_);
            stage_ = Stage::ModifyNew;
            ++t_;
            break;
        case Stage::ModifyNew:
            finish(wanted);
            break;
        }
    }
    pollLast_ = nmiLatched_ || (irq_ && (r_.p & kI) == 0);
    if (fetch) {
        pollFetch_ = pollLast_;
    }
}

void Mos6502Core::finish(bool interrupt) {
    t_ = 1;
    stage_ = Stage::Addressing;
    interruptNext_ = interrupt;
    bus_ = {r_.pc, 0, true, true};
}

void Mos6502Core::addressing(std::uint8_t data) {
    const Opcode& o = kOpcodes[opcode_];
    const bool wanted = pollLast_;
    const int t = t_++;
    switch (o.mode) {
    case Mode::Implied:
        impliedOperation();
        finish(wanted);
        return;
    case Mode::Accumulator:
        r_.a = modifyOperation(r_.a);
        finish(wanted);
        return;
    case Mode::Immediate:
        ++r_.pc;
        readOperation(data);
        finish(wanted);
        return;
    case Mode::ZeroPage:
        ++r_.pc;
        beginAccess(data);
        return;
    case Mode::ZeroPageX:
    case Mode::ZeroPageY:
        if (t == 2) {
            ++r_.pc;
            adl_ = data;
            read(adl_);
        } else {
            const std::uint8_t index = o.mode == Mode::ZeroPageX ? r_.x : r_.y;
            beginAccess(static_cast<std::uint8_t>(adl_ + index));
        }
        return;
    case Mode::Absolute:
        ++r_.pc;
        if (t == 2) {
            adl_ = data;
            read(r_.pc);
        } else {
            beginAccess(Word(data, adl_));
        }
        return;
    case Mode::AbsoluteX:
    case Mode::AbsoluteY: {
        const std::uint8_t index = o.mode == Mode::AbsoluteX ? r_.x : r_.y;
        if (t == 2) {
            ++r_.pc;
            adl_ = data;
            read(r_.pc);
        } else if (t == 3) {
            ++r_.pc;
            adh_ = data;
            indexed(index);
        } else {
            fixedAccess();
        }
        return;
    }
    case Mode::IndirectX:
        if (t == 2) {
            ++r_.pc;
            pointer_ = data;
            read(pointer_);
        } else if (t == 3) {
            pointer_ = static_cast<std::uint8_t>(pointer_ + r_.x);
            read(pointer_);
        } else if (t == 4) {
            adl_ = data;
            read(static_cast<std::uint8_t>(pointer_ + 1));
        } else {
            beginAccess(Word(data, adl_));
        }
        return;
    case Mode::IndirectY:
        if (t == 2) {
            ++r_.pc;
            pointer_ = data;
            read(pointer_);
        } else if (t == 3) {
            adl_ = data;
            read(static_cast<std::uint8_t>(pointer_ + 1));
        } else if (t == 4) {
            adh_ = data;
            indexed(r_.y);
        } else {
            fixedAccess();
        }
        return;
    case Mode::Relative:
        if (t == 2) {
            ++r_.pc;
            value_ = data;
            if (!branchTaken()) {
                finish(wanted);
            } else {
                read(r_.pc);
            }
        } else if (t == 3) {
            const auto target =
                static_cast<std::uint16_t>(r_.pc + static_cast<std::int8_t>(value_));
            if (High(target) == High(r_.pc)) {
                r_.pc = target;
                finish(pollFetch_);  // a taken branch in its page polls only during its fetch
            } else {
                read(Word(High(r_.pc), Low(target)));
                r_.pc = target;
            }
        } else {
            finish(wanted);
        }
        return;
    case Mode::JumpAbsolute:
        if (t == 2) {
            ++r_.pc;
            adl_ = data;
            read(r_.pc);
        } else {
            r_.pc = Word(data, adl_);
            finish(wanted);
        }
        return;
    case Mode::JumpIndirect:
        if (t == 2) {
            ++r_.pc;
            adl_ = data;
            read(r_.pc);
        } else if (t == 3) {
            ++r_.pc;
            adh_ = data;
            read(Word(adh_, adl_));
        } else if (t == 4) {
            value_ = data;
            read(Word(adh_, static_cast<std::uint8_t>(adl_ + 1)));  // no carry into the page
        } else {
            r_.pc = Word(data, value_);
            finish(wanted);
        }
        return;
    case Mode::Call:
        if (t == 2) {
            ++r_.pc;
            adl_ = data;
            read(Stack(r_.s));
        } else if (t == 3) {
            write(Stack(r_.s--), High(r_.pc));
        } else if (t == 4) {
            write(Stack(r_.s--), Low(r_.pc));
        } else if (t == 5) {
            read(r_.pc);
        } else {
            r_.pc = Word(data, adl_);
            finish(wanted);
        }
        return;
    case Mode::Return:
        if (t == 2) {
            read(Stack(r_.s));
        } else if (t == 3) {
            read(Stack(++r_.s));
        } else if (t == 4) {
            adl_ = data;
            read(Stack(++r_.s));
        } else if (t == 5) {
            r_.pc = Word(data, adl_);
            read(r_.pc);
        } else {
            ++r_.pc;
            finish(wanted);
        }
        return;
    case Mode::ReturnFromInterrupt:
        if (t == 2) {
            read(Stack(r_.s));
        } else if (t == 3) {
            read(Stack(++r_.s));
        } else if (t == 4) {
            r_.p = static_cast<std::uint8_t>((data | kU) & ~kB);
            read(Stack(++r_.s));
        } else if (t == 5) {
            adl_ = data;
            read(Stack(++r_.s));
        } else {
            r_.pc = Word(data, adl_);
            finish(wanted);
        }
        return;
    case Mode::Push:
        if (t == 2) {
            const std::uint8_t value =
                o.op == Op::Pha ? r_.a : static_cast<std::uint8_t>(r_.p | kB | kU);
            write(Stack(r_.s--), value);
        } else {
            finish(wanted);
        }
        return;
    case Mode::Pull:
        if (t == 2) {
            read(Stack(r_.s));
        } else if (t == 3) {
            read(Stack(++r_.s));
        } else {
            if (o.op == Op::Pla) {
                r_.a = data;
                setNz(data);
            } else {
                r_.p = static_cast<std::uint8_t>((data | kU) & ~kB);
            }
            finish(wanted);
        }
        return;
    case Mode::Break: {
        const bool reset = breakKind_ == BreakKind::Reset;
        const auto push = [&](std::uint8_t value) {
            if (reset) {
                read(Stack(r_.s--));
            } else {
                write(Stack(r_.s--), value);
            }
        };
        if (t == 2) {
            if (breakKind_ == BreakKind::Break) {
                ++r_.pc;
            }
            push(High(r_.pc));
        } else if (t == 3) {
            push(Low(r_.pc));
        } else if (t == 4) {
            // The vector is chosen now: a latched NMI takes over a BRK or an IRQ.
            if (reset) {
                address_ = kResetVector;
            } else if (nmiLatched_) {
                nmiLatched_ = false;
                address_ = kNmiVector;
            } else {
                address_ = kIrqVector;
            }
            const std::uint8_t b = breakKind_ == BreakKind::Break ? kB : 0;
            push(static_cast<std::uint8_t>((r_.p & ~kB) | b | kU));
        } else if (t == 5) {
            r_.p |= kI;
            read(address_);
        } else if (t == 6) {
            adl_ = data;
            read(static_cast<std::uint16_t>(address_ + 1));
        } else {
            r_.pc = Word(data, adl_);
            finish(false);  // the handler's first instruction always runs
        }
        return;
    }
    case Mode::Jam:
        jammed_ = true;
        jamCycle_ = 1;
        read(0xFFFF);
        return;
    }
}

// T3 of abs,X/Y (T4 of (zp),Y): the low byte added, the high byte not yet fixed.
void Mos6502Core::indexed(std::uint8_t index) {
    const unsigned low = adl_ + index;
    crossed_ = low > 0xFF;
    address_ = Word(adh_, static_cast<std::uint8_t>(low));
    const Opcode& o = kOpcodes[opcode_];
    if (o.access == Access::Read && !crossed_) {
        beginAccess(address_);
    } else {
        read(address_);  // dummy read at the unfixed address
    }
}

void Mos6502Core::fixedAccess() {
    const auto fixed = static_cast<std::uint16_t>(address_ + (crossed_ ? 0x100 : 0));
    const Op op = kOpcodes[opcode_].op;
    if (op == Op::Sha || op == Op::Shx || op == Op::Shy || op == Op::Tas) {
        // Stores reg & (H + 1), H the base's high byte; a carried index puts that value in the
        // address's high byte too.
        if (op == Op::Tas) {
            r_.s = r_.a & r_.x;
        }
        const std::uint8_t source = op == Op::Shx   ? r_.x
                                    : op == Op::Shy ? r_.y
                                    : op == Op::Tas ? r_.s
                                                    : static_cast<std::uint8_t>(r_.a & r_.x);
        value_ = static_cast<std::uint8_t>(source & (adh_ + 1));
        address_ = crossed_ ? Word(value_, Low(fixed)) : fixed;
        stage_ = Stage::Access;
        write(address_, value_);
        return;
    }
    beginAccess(fixed);
}

void Mos6502Core::beginAccess(std::uint16_t address) {
    address_ = address;
    stage_ = Stage::Access;
    if (kOpcodes[opcode_].access == Access::Write) {
        write(address, storeValue());
    } else {
        read(address);
    }
}

void Mos6502Core::endAccess(std::uint8_t data) {
    const Opcode& o = kOpcodes[opcode_];
    ++t_;
    switch (o.access) {
    case Access::Read:
        readOperation(data);
        finish(pollLast_);
        return;
    case Access::Modify:
        value_ = data;
        write(address_, value_);  // the NMOS chip writes the old value back first
        stage_ = Stage::ModifyOld;
        return;
    default:
        finish(pollLast_);
        return;
    }
}

std::uint8_t Mos6502Core::storeValue() {
    switch (kOpcodes[opcode_].op) {
    case Op::Sta:
        return r_.a;
    case Op::Stx:
        return r_.x;
    case Op::Sty:
        return r_.y;
    case Op::Sax:
        return r_.a & r_.x;
    case Op::Sha:  // (zp),Y without a carry reaches here only through fixedAccess
    default:
        return r_.a & r_.x;
    }
}

void Mos6502Core::setNz(std::uint8_t value) {
    r_.p = static_cast<std::uint8_t>((r_.p & ~(kN | kZ)) | (value & kN) | (value == 0 ? kZ : 0));
}

void Mos6502Core::compare(std::uint8_t reg, std::uint8_t value) {
    const auto difference = static_cast<std::uint8_t>(reg - value);
    setNz(difference);
    r_.p = static_cast<std::uint8_t>((r_.p & ~kC) | (reg >= value ? kC : 0));
}

// NMOS decimal mode (Bruce Clark, "Decimal Mode", Appendix A): Z from the binary sum, N and V
// from the sum after the low-digit fix.
void Mos6502Core::adc(std::uint8_t value) {
    const unsigned a = r_.a, carry = r_.p & kC;
    const unsigned binary = a + value + carry;
    std::uint8_t p = r_.p & ~(kN | kV | kZ | kC);
    if ((r_.p & kD) == 0) {
        p |= (binary & 0x80) | ((binary & 0xFF) == 0 ? kZ : 0) | (binary > 0xFF ? kC : 0);
        p |= (~(a ^ value) & (a ^ binary) & 0x80) != 0 ? kV : 0;
        r_.a = static_cast<std::uint8_t>(binary);
        r_.p = p;
        return;
    }
    unsigned low = (a & 0x0F) + (value & 0x0F) + carry;
    unsigned high = (a & 0xF0) + (value & 0xF0);
    if (low > 0x09) {
        high += 0x10;
        low += 0x06;
    }
    p |= (binary & 0xFF) == 0 ? kZ : 0;
    p |= high & 0x80;
    p |= (~(a ^ value) & (a ^ high) & 0x80) != 0 ? kV : 0;
    if (high > 0x90) {
        high += 0x60;
    }
    p |= high > 0xFF ? kC : 0;
    r_.a = static_cast<std::uint8_t>((low & 0x0F) | (high & 0xF0));
    r_.p = p;
}

// Flags always from the binary difference; the result in BCD when D is set.
void Mos6502Core::sbc(std::uint8_t value) {
    const unsigned a = r_.a, borrow = (r_.p & kC) != 0 ? 0 : 1;
    const unsigned binary = a - value - borrow;
    std::uint8_t p = r_.p & ~(kN | kV | kZ | kC);
    p |= (binary & 0x80) | ((binary & 0xFF) == 0 ? kZ : 0) | (binary < 0x100 ? kC : 0);
    p |= ((a ^ value) & (a ^ binary) & 0x80) != 0 ? kV : 0;
    r_.p = p;
    if ((p & kD) == 0) {
        r_.a = static_cast<std::uint8_t>(binary);
        return;
    }
    unsigned low = (a & 0x0F) - (value & 0x0F) - borrow;
    unsigned high = (a & 0xF0) - (value & 0xF0);
    if ((low & 0x10) != 0) {
        low -= 6;
        high -= 0x10;
    }
    if ((high & 0x100) != 0) {
        high -= 0x60;
    }
    r_.a = static_cast<std::uint8_t>((low & 0x0F) | (high & 0xF0));
}

void Mos6502Core::readOperation(std::uint8_t value) {
    switch (kOpcodes[opcode_].op) {
    case Op::Lda:
        r_.a = value;
        setNz(value);
        break;
    case Op::Ldx:
        r_.x = value;
        setNz(value);
        break;
    case Op::Ldy:
        r_.y = value;
        setNz(value);
        break;
    case Op::Lax:
        r_.a = r_.x = value;
        setNz(value);
        break;
    case Op::Adc:
        adc(value);
        break;
    case Op::Sbc:
        sbc(value);
        break;
    case Op::And:
        r_.a &= value;
        setNz(r_.a);
        break;
    case Op::Ora:
        r_.a |= value;
        setNz(r_.a);
        break;
    case Op::Eor:
        r_.a ^= value;
        setNz(r_.a);
        break;
    case Op::Cmp:
        compare(r_.a, value);
        break;
    case Op::Cpx:
        compare(r_.x, value);
        break;
    case Op::Cpy:
        compare(r_.y, value);
        break;
    case Op::Bit:
        r_.p = static_cast<std::uint8_t>((r_.p & ~(kN | kV | kZ)) | (value & (kN | kV)) |
                                         ((r_.a & value) == 0 ? kZ : 0));
        break;
    case Op::Las:
        r_.a = r_.x = r_.s = value & r_.s;
        setNz(r_.a);
        break;
    case Op::Anc:
        r_.a &= value;
        setNz(r_.a);
        r_.p = static_cast<std::uint8_t>((r_.p & ~kC) | ((r_.a & 0x80) != 0 ? kC : 0));
        break;
    case Op::Alr: {
        const auto t = static_cast<std::uint8_t>(r_.a & value);
        r_.p = static_cast<std::uint8_t>((r_.p & ~kC) | (t & kC));
        r_.a = static_cast<std::uint8_t>(t >> 1);
        setNz(r_.a);
        break;
    }
    case Op::Arr: {
        const auto t = static_cast<std::uint8_t>(r_.a & value);
        const unsigned carry = r_.p & kC;
        auto result = static_cast<std::uint8_t>((t >> 1) | (carry << 7));
        std::uint8_t p = r_.p & ~(kN | kV | kZ | kC);
        if ((r_.p & kD) == 0) {
            p |= (result & kN) | (result == 0 ? kZ : 0);
            p |= (result & 0x40) != 0 ? kC : 0;
            p |= ((result >> 6) ^ (result >> 5)) & 1 ? kV : 0;
        } else {
            // "No More Secrets": N and Z of the binary result, V from bit 6 changing, then the
            // BCD fix of each digit; C from the high digit's fix.
            p |= (result & kN) | (result == 0 ? kZ : 0);
            p |= ((t ^ result) & 0x40) != 0 ? kV : 0;
            if ((t & 0x0F) + (t & 0x01) > 5) {
                result = static_cast<std::uint8_t>((result & 0xF0) | ((result + 6) & 0x0F));
            }
            if ((t & 0xF0) + (t & 0x10) > 0x50) {
                result = static_cast<std::uint8_t>(result + 0x60);
                p |= kC;
            }
        }
        r_.a = result;
        r_.p = p;
        break;
    }
    case Op::Sbx: {
        const auto t = static_cast<std::uint8_t>(r_.a & r_.x);
        r_.x = static_cast<std::uint8_t>(t - value);
        r_.p = static_cast<std::uint8_t>((r_.p & ~kC) | (t >= value ? kC : 0));
        setNz(r_.x);
        break;
    }
    case Op::Ane:
        r_.a = static_cast<std::uint8_t>((r_.a | 0xEE) & r_.x & value);
        setNz(r_.a);
        break;
    case Op::Lxa:
        r_.a = r_.x = static_cast<std::uint8_t>((r_.a | 0xEE) & value);
        setNz(r_.a);
        break;
    default:  // Nop
        break;
    }
}

std::uint8_t Mos6502Core::modifyOperation(std::uint8_t value) {
    const auto carryIn = static_cast<unsigned>(r_.p & kC);
    const auto setCarry = [this](bool c) {
        r_.p = static_cast<std::uint8_t>((r_.p & ~kC) | (c ? kC : 0));
    };
    std::uint8_t result = value;
    const Op op = kOpcodes[opcode_].op;
    switch (op) {
    case Op::Asl:
    case Op::Slo:
        setCarry((value & 0x80) != 0);
        result = static_cast<std::uint8_t>(value << 1);
        break;
    case Op::Lsr:
    case Op::Sre:
        setCarry((value & 0x01) != 0);
        result = static_cast<std::uint8_t>(value >> 1);
        break;
    case Op::Rol:
    case Op::Rla:
        setCarry((value & 0x80) != 0);
        result = static_cast<std::uint8_t>(value << 1 | carryIn);
        break;
    case Op::Ror:
    case Op::Rra:
        setCarry((value & 0x01) != 0);
        result = static_cast<std::uint8_t>(value >> 1 | carryIn << 7);
        break;
    case Op::Inc:
    case Op::Isc:
        result = static_cast<std::uint8_t>(value + 1);
        break;
    case Op::Dec:
    case Op::Dcp:
        result = static_cast<std::uint8_t>(value - 1);
        break;
    default:
        break;
    }
    switch (op) {
    case Op::Slo:
        r_.a |= result;
        setNz(r_.a);
        break;
    case Op::Rla:
        r_.a &= result;
        setNz(r_.a);
        break;
    case Op::Sre:
        r_.a ^= result;
        setNz(r_.a);
        break;
    case Op::Rra:
        adc(result);
        break;
    case Op::Dcp:
        compare(r_.a, result);
        break;
    case Op::Isc:
        sbc(result);
        break;
    default:
        setNz(result);
        break;
    }
    return result;
}

void Mos6502Core::impliedOperation() {
    switch (kOpcodes[opcode_].op) {
    case Op::Clc:
        r_.p &= ~kC;
        break;
    case Op::Sec:
        r_.p |= kC;
        break;
    case Op::Cli:
        r_.p &= ~kI;
        break;
    case Op::Sei:
        r_.p |= kI;
        break;
    case Op::Clv:
        r_.p &= ~kV;
        break;
    case Op::Cld:
        r_.p &= ~kD;
        break;
    case Op::Sed:
        r_.p |= kD;
        break;
    case Op::Tax:
        r_.x = r_.a;
        setNz(r_.x);
        break;
    case Op::Txa:
        r_.a = r_.x;
        setNz(r_.a);
        break;
    case Op::Tay:
        r_.y = r_.a;
        setNz(r_.y);
        break;
    case Op::Tya:
        r_.a = r_.y;
        setNz(r_.a);
        break;
    case Op::Tsx:
        r_.x = r_.s;
        setNz(r_.x);
        break;
    case Op::Txs:
        r_.s = r_.x;
        break;
    case Op::Inx:
        setNz(++r_.x);
        break;
    case Op::Iny:
        setNz(++r_.y);
        break;
    case Op::Dex:
        setNz(--r_.x);
        break;
    case Op::Dey:
        setNz(--r_.y);
        break;
    default:  // Nop
        break;
    }
}

bool Mos6502Core::branchTaken() const {
    const std::uint8_t p = r_.p;
    switch (kOpcodes[opcode_].op) {
    case Op::Bpl:
        return (p & kN) == 0;
    case Op::Bmi:
        return (p & kN) != 0;
    case Op::Bvc:
        return (p & kV) == 0;
    case Op::Bvs:
        return (p & kV) != 0;
    case Op::Bcc:
        return (p & kC) == 0;
    case Op::Bcs:
        return (p & kC) != 0;
    case Op::Bne:
        return (p & kZ) == 0;
    default:  // Beq
        return (p & kZ) != 0;
    }
}

Mos6502Instruction Mos6502Decode(std::uint8_t opcode) {
    const Opcode& o = kOpcodes[opcode];
    const bool documented =
        IsDocumentedOp(o.op) && (o.op != Op::Nop || opcode == 0xEA) && opcode != 0xEB;
    return {kMnemonics[static_cast<std::size_t>(o.op)], OperandOf(o.mode), documented};
}

}  // namespace bw
