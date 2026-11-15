#pragma once

#include <cstdint>
#include <span>

namespace bw {

// The MOS 6522 VIA's registers and logic, one φ2 cycle at a time (P5n,
// docs/p5n-bw2001-chips/BW2001_CHIPS_DESIGN.md §6). No simulator types. A cycle is rise(inputs),
// then read() for a selected read, then write() for a selected write (at the fall), then fall().
class Mos6522Core {
public:
    struct Inputs {  // pin levels sampled at this cycle's φ2 rise
        std::uint8_t pa = 0xFF, pb = 0xFF;
        bool ca1 = true, ca2 = true, cb1 = true, cb2 = true;
    };
    struct Outputs {                            // what the chip drives after this edge
        std::uint8_t paDrive = 0, pbDrive = 0;  // 1 bits: drive the bit of paLevel / pbLevel
        std::uint8_t paLevel = 0, pbLevel = 0;
        bool ca2Drive = false, ca2 = true;
        bool cb1Drive = false, cb1 = true;
        bool cb2Drive = false, cb2 = true;
        bool cb2Shift = false;  // CB2 is the shift register's (its own delay)
        bool pb7Timer = false;  // PB7 is T1's
        bool irq = false;       // IRQ_N low
    };

    void reset();  // RES_N low: every register but T1, T2 and SR (R6522 p. 2)
    void randomise(std::span<const std::uint8_t, 16> bytes);  // power-on
    void rise(const Inputs& in);
    std::uint8_t read(unsigned reg);           // in the rise, after rise()
    void write(unsigned reg, std::uint8_t v);  // at the fall, before fall()
    void fall();
    Outputs outputs() const;

    // Tests.
    std::uint16_t t1() const { return t1Counter_; }
    std::uint16_t t2() const { return t2Counter_; }
    std::uint8_t sr() const { return sr_; }
    std::uint8_t ifr() const { return ifr_; }

private:
    unsigned srMode() const { return (acr_ >> 2) & 7u; }
    unsigned ca2Mode() const { return (pcr_ >> 1) & 7u; }
    unsigned cb2Mode() const { return (pcr_ >> 5) & 7u; }
    bool cb1IsOutput() const;
    void clearA();  // a register 1 access: the CA1 flag, and CA2's unless independent
    void clearB();  // a register 0 access: CB1, and CB2 unless independent
    void startShift();
    void shiftEdge(bool cb1Rising);  // the SR's clock moved (internal or external)

    std::uint8_t ora_ = 0, orb_ = 0, ddra_ = 0, ddrb_ = 0, acr_ = 0, pcr_ = 0, ifr_ = 0, ier_ = 0;
    std::uint8_t ira_ = 0, irb_ = 0;  // latched on C1 edges (ACR bits 0, 1)
    std::uint16_t t1Counter_ = 0, t1Latch_ = 0, t2Counter_ = 0;
    std::uint8_t t2LatchLow_ = 0;
    std::uint8_t sr_ = 0;
    Inputs in_{};
    bool sampled_ = false;
    bool t1Loaded_ = false, t1Reload_ = false, t1Armed_ = false, pb7_ = true;
    bool t2Loaded_ = false, t2Armed_ = false;
    // C2 strobes: start at this fall (a read) or the next rise (a write); a pulse ends one cycle
    // after it started, at the same kind of edge.
    bool ca2Out_ = true, ca2StrobeFall_ = false, ca2StrobeRise_ = false;
    bool ca2EndFall_ = false, ca2EndRise_ = false;
    bool cb2Out_ = true, cb2StrobeRise_ = false, cb2EndRise_ = false;
    // Shift register.
    bool srRunning_ = false, srStarted_ = false, cb1Out_ = true, srCb2_ = true;
    unsigned srBits_ = 0, srDivider_ = 0;
};

}  // namespace bw
