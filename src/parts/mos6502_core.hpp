#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bw {

// The programmer's registers. p: N V 1 B D I Z C; bit 5 and B are kept as given or pulled.
struct Mos6502Registers {
    std::uint16_t pc = 0;
    std::uint8_t a = 0, x = 0, y = 0, s = 0, p = 0;
};

// How an instruction's operand is written in assembly.
enum class Mos6502Operand : std::uint8_t {
    None,
    Accumulator,
    Immediate,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    Indirect,
    IndirectX,
    IndirectY,
    Relative
};

// An opcode as an assembler sees it, from the core's own table (so the two cannot disagree).
struct Mos6502Instruction {
    std::string_view mnemonic;  // upper case; undocumented ones by their usual names
    Mos6502Operand operand = Mos6502Operand::None;
    bool documented = false;  // one of the 151 in the MOS data sheet
};

Mos6502Instruction Mos6502Decode(std::uint8_t opcode);
// Bytes after the opcode: 0, 1 or 2 (the assembler's and the disassembler's, P6p).
std::size_t Mos6502OperandBytes(Mos6502Operand operand);

// One bus cycle as the CPU drives it.
struct Mos6502Bus {
    std::uint16_t address = 0;
    std::uint8_t data = 0;  // written; meaningless on a read
    bool read = true;       // RW high
    bool sync = false;      // opcode fetch
};

// NMOS 6502 instruction core, one bus cycle at a time (docs/p5c-6502/MOS6502_DESIGN.md): every
// opcode, documented or not, with the bus of each cycle as on the chip (64doc). No simulator
// types: the 6502 Part (mos6502.hpp) puts it behind the Pins; the tests run it on a byte array.
class Mos6502Core {
public:
    void start(const Mos6502Registers& registers);  // next cycle: an opcode fetch at registers.pc
    const Mos6502Bus& bus() const { return bus_; }  // the cycle in progress
    void step(std::uint8_t data);                   // ends it; `data` read (ignored on a write)
    const Mos6502Registers& registers() const { return r_; }
    bool jammed() const { return jammed_; }

    // Input levels seen by the next step(); true = asserted (the pin low).
    void setIrq(bool asserted) { irq_ = asserted; }
    void setNmi(bool asserted);  // latches on the deasserted → asserted change
    void setReset(bool asserted) { reset_ = asserted; }
    void setOverflow();  // SO's falling edge: V = 1 now

    enum class BreakKind : std::uint8_t { Break, Irq, Nmi, Reset };

private:
    enum class Stage : std::uint8_t { Addressing, Access, ModifyOld, ModifyNew };

    void addressing(std::uint8_t data);
    void beginAccess(std::uint16_t address);
    void endAccess(std::uint8_t data);
    void finish(bool interrupt);
    void read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t data);
    void indexed(std::uint8_t index);  // T3/T4 of an absolute or (zp),Y access: the low-byte add
    void fixedAccess();                // the cycle after the unfixed one
    std::uint8_t storeValue();
    void readOperation(std::uint8_t value);
    std::uint8_t modifyOperation(std::uint8_t value);
    void impliedOperation();
    bool branchTaken() const;
    void setNz(std::uint8_t value);
    void adc(std::uint8_t value);
    void sbc(std::uint8_t value);
    void compare(std::uint8_t reg, std::uint8_t value);

    Mos6502Registers r_;
    Mos6502Bus bus_;
    std::uint8_t opcode_ = 0xEA;
    int t_ = 1;  // cycle within the instruction, 1 = the opcode fetch
    Stage stage_ = Stage::Addressing;
    BreakKind breakKind_ = BreakKind::Break;
    std::uint8_t adl_ = 0, adh_ = 0, pointer_ = 0, value_ = 0;
    std::uint16_t address_ = 0;  // effective address
    bool crossed_ = false;       // the index add carried into the high byte
    int jamCycle_ = 0;

    bool irq_ = false, nmiLevel_ = false, nmiLatched_ = false, reset_ = false;
    bool resetHeld_ = false, jammed_ = false;
    bool interruptNext_ = false;  // the coming fetch is replaced by an interrupt sequence
    bool pollLast_ = false;       // interrupt wanted at the end of the last cycle
    bool pollFetch_ = false;      // ... at the end of this instruction's fetch
};

}  // namespace bw
