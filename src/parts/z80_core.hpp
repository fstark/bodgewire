#pragma once

#include <cstdint>

namespace bw {

// The programmer's registers, and the hidden state Tom Harte's tests record.
struct Z80Registers {
    std::uint16_t pc = 0, sp = 0, ix = 0, iy = 0;
    std::uint16_t wz = 0;  // MEMPTR
    std::uint8_t a = 0, f = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0, i = 0, r = 0;
    std::uint16_t af2 = 0, bc2 = 0, de2 = 0, hl2 = 0;  // the alternate set
    std::uint8_t im = 0;
    std::uint8_t q = 0;  // F if the last instruction set the flags, else 0 (SCF, CCF)
    bool iff1 = false, iff2 = false;
    bool ei = false;  // the last instruction was EI: no interrupt after it
    bool p = false;   // the last instruction was LD A,I or LD A,R (the NMOS P/V loss)
};

// One machine cycle (M-cycle) as the pins show it.
enum class Z80CycleKind : std::uint8_t {
    Fetch,        // M1: opcode read, then refresh at I:R (4 T-states)
    Read,         // memory read (3)
    Write,        // memory write (3)
    In,           // I/O read, one automatic wait included (4)
    Out,          // I/O write (4)
    Acknowledge,  // INT acknowledge: M1 with IORQ, two automatic waits included (6)
    Internal      // `length` T-states with nothing on the bus (the address held)
};

// T-states of a cycle, wait states (WAIT_N) excluded.
constexpr int Z80CycleLength(Z80CycleKind kind, int internalLength) {
    switch (kind) {
    case Z80CycleKind::Fetch:
    case Z80CycleKind::In:
    case Z80CycleKind::Out:
        return 4;
    case Z80CycleKind::Read:
    case Z80CycleKind::Write:
        return 3;
    case Z80CycleKind::Acknowledge:
        return 6;
    case Z80CycleKind::Internal:
        return internalLength;
    }
    return 0;
}

struct Z80Bus {
    Z80CycleKind kind = Z80CycleKind::Fetch;
    std::uint16_t address = 0;  // Internal: unchanged from the cycle before
    std::uint8_t data = 0;      // Write, Out
    std::uint16_t refresh = 0;  // Fetch, Acknowledge: I:R as driven in T3 and T4
    std::uint8_t length = 0;    // Internal: its T-states
    bool halted = false;        // Fetch: a halted CPU's NOP at the HALT (data ignored, PC kept)
};

// NMOS Z80 instruction core, one M-cycle at a time (docs/p5s-z80/Z80_DESIGN.md): every opcode
// of the five tables, documented or not, with each cycle as on the chip and the undocumented
// flags, MEMPTR and Q as Tom Harte's data records them. No simulator types: the Z80 Part (z80.hpp)
// puts it behind the Pins; the tests run it on a byte array.
class Z80Core {
public:
    void start(const Z80Registers& registers);  // next cycle: a fetch at registers.pc
    void reset();  // the reset values (PC, I, R 0, interrupts off, IM 0, AF SP $FFFF); next: a
                   // fetch at $0000
    const Z80Bus& bus() const { return bus_; }  // the M-cycle in progress
    void step(std::uint8_t data);               // ends it; `data` read (ignored but on reads)
    Z80Registers registers() const;             // between instructions: as Harte's `final`
    bool halted() const { return halted_; }
    bool instructionEnded() const { return ended_; }  // the last step ended an instruction

    // Inputs for the next step(), true = asserted (the pin low). INT is a level, looked at when an
    // instruction ends; NMI latches on the deasserted → asserted change.
    void setInt(bool asserted) { int_ = asserted; }
    void setNmi(bool asserted);

private:
    void run();
    void fetch();
    void haltedFetch();
    void read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t data);
    void in(std::uint16_t port);
    void out(std::uint16_t port, std::uint8_t data);
    void internal(int length);
    void acknowledge();
    std::uint16_t pcNext();

    std::uint8_t a() const { return static_cast<std::uint8_t>(af_ >> 8); }
    std::uint8_t f() const { return static_cast<std::uint8_t>(af_); }
    void setA(std::uint8_t value) { af_ = static_cast<std::uint16_t>(value << 8 | f()); }
    void setF(std::uint8_t value);  // and Q
    std::uint16_t& hl();            // HL, IX or IY by the prefix
    std::uint16_t& rp(int p);       // BC DE HL SP
    std::uint16_t& rp2(int p);      // BC DE HL AF
    std::uint8_t reg(int r) const;  // B C D E H L - A; H and L by the prefix
    void setReg(int r, std::uint8_t value);
    std::uint8_t plainReg(int r) const;  // B C D E H L - A, never IXH/IXL
    void setPlainReg(int r, std::uint8_t value);
    bool condition(int y) const;

    void alu(int op, std::uint8_t value);  // ADD ADC SUB SBC AND XOR OR CP
    std::uint8_t inc(std::uint8_t value);
    std::uint8_t dec(std::uint8_t value);
    std::uint8_t rotate(int op, std::uint8_t value);  // RLC RRC RL RR SLA SRA SLL SRL
    void bit(int b, std::uint8_t value, std::uint8_t xy);
    void accumulatorOp(int y);  // RLCA RRCA RLA RRA DAA CPL SCF CCF
    void add16(std::uint16_t value);
    void adc16(std::uint16_t value);
    void sbc16(std::uint16_t value);
    void blockLoadFlags(std::uint8_t value);
    void blockCompareFlags(std::uint8_t value);
    void blockIoFlags(std::uint8_t value, unsigned k);
    void blockRepeatFlags();
    void blockIoRepeatFlags(std::uint8_t value, unsigned k);

    Z80Bus bus_;
    int resume_ = 0;
    std::uint8_t data_ = 0;

    std::uint16_t af_ = 0, bc_ = 0, de_ = 0, hl_ = 0, ix_ = 0, iy_ = 0, sp_ = 0, pc_ = 0, wz_ = 0;
    std::uint16_t af2_ = 0, bc2_ = 0, de2_ = 0, hl2_ = 0;
    std::uint8_t i_ = 0, r_ = 0, im_ = 0, q_ = 0;
    bool iff1_ = false, iff2_ = false, ei_ = false, p_ = false;

    // The instruction in progress.
    std::uint8_t op_ = 0;
    int index_ = 0;  // 0 HL, 1 IX, 2 IY
    std::uint16_t address_ = 0;
    std::uint8_t value_ = 0;
    bool flagsSet_ = false, eiNow_ = false, pNow_ = false;
    bool im0_ = false;  // executing the opcode an IM 0 acknowledge read: PC not counted

    bool halted_ = false, ended_ = false;
    bool int_ = false, nmiLevel_ = false, nmiLatched_ = false;
};

}  // namespace bw
