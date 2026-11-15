#include "parts/mos6520.hpp"

#include <utility>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using namespace delays;

constexpr LogicValue Bit(unsigned value, unsigned bit) { return (value >> bit) & 1 ? One : Zero; }

// CR bits.
constexpr std::uint8_t kC1Enable = 0x01, kC1Rising = 0x02, kSelectOr = 0x04, kC2Bit3 = 0x08,
                       kC2Bit4 = 0x10, kC2Output = 0x20;

bool C2Output(std::uint8_t control) { return (control & kC2Output) != 0; }
// C2 output modes by bits 4 3: 00 handshake, 01 pulse, 1x manual (bit 3 the level).
bool C2Strobed(std::uint8_t control) { return C2Output(control) && (control & kC2Bit4) == 0; }
bool C2Pulse(std::uint8_t control) { return C2Strobed(control) && (control & kC2Bit3) != 0; }

}  // namespace

std::vector<PinSpec> Mos6520::Pins() {
    // NMOS: every signal pin MOS (X read as a random bit). Only PHI2 wakes the chip.
    const auto in = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Mos, false, true};  // sampled
    };
    const auto data = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Mos, false, true};
    };
    const auto pullUp = [](const char* name) {
        return PinSpec{name, PinKind::PullUp, Family::Mos, false, true};
    };
    const auto irq = [](const char* name) {
        return PinSpec{name, PinKind::OpenCollector, Family::Mos};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {power("GND"),
            pullUp("PA0"),
            pullUp("PA1"),
            pullUp("PA2"),
            pullUp("PA3"),
            pullUp("PA4"),
            pullUp("PA5"),
            pullUp("PA6"),
            pullUp("PA7"),
            data("PB0"),
            data("PB1"),
            data("PB2"),
            data("PB3"),
            data("PB4"),
            data("PB5"),
            data("PB6"),
            data("PB7"),
            in("CB1"),
            data("CB2"),
            power("VCC"),
            in("RW"),
            in("CS0"),
            in("CS2_N"),
            in("CS1"),
            PinSpec{"PHI2", PinKind::Input, Family::Mos},
            data("D7"),
            data("D6"),
            data("D5"),
            data("D4"),
            data("D3"),
            data("D2"),
            data("D1"),
            data("D0"),
            in("RES_N"),
            in("RS1"),
            in("RS0"),
            irq("IRQB_N"),
            irq("IRQA_N"),
            pullUp("CA2"),
            in("CA1")};
}

Mos6520::Mos6520(std::string reference) : Component(std::move(reference), Pins()) {}

void Mos6520::powerOn(PowerOnContext& ctx) {
    // A real NMOS chip powers up undefined until RES; this one with random registers.
    const auto byte = [&ctx] {
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            value |= (ctx.randomBit() ? 1u : 0u) << bit;
        }
        return static_cast<std::uint8_t>(value);
    };
    for (Side& s : side_) {
        s = Side{};
        s.output = byte();
        s.direction = byte();
        s.control = byte() & 0x3F;
        s.c2Out = !C2Output(s.control) || (s.control & kC2Bit4) == 0 || (s.control & kC2Bit3);
    }
    sampled_ = driving_ = false;
    driven_.fill(Z);
    // The Pins from the registers, as driveOutputs() would, without events.
    for (unsigned bit = 0; bit < 8; ++bit) {
        const bool outA = (side_[0].direction >> bit) & 1;
        driven_[kPortA[bit]] = outA && !((side_[0].output >> bit) & 1) ? Zero : One;
        const bool outB = (side_[1].direction >> bit) & 1;
        driven_[kPortB[bit]] = outB ? Bit(side_[1].output, bit) : Z;
        ctx.setOutput(kPortA[bit], driven_[kPortA[bit]]);
        ctx.setOutput(kPortB[bit], driven_[kPortB[bit]]);
    }
    driven_[kCa2] = C2Output(side_[0].control) && !side_[0].c2Out ? Zero : One;
    driven_[kCb2] = C2Output(side_[1].control) ? (side_[1].c2Out ? One : Zero) : Z;
    driven_[kIrqAN] = driven_[kIrqBN] = One;
    ctx.setOutput(kCa2, driven_[kCa2]);
    ctx.setOutput(kCb2, driven_[kCb2]);
    ctx.setOutput(kIrqAN, One);
    ctx.setOutput(kIrqBN, One);
}

void Mos6520::evaluate(EvalContext& ctx) {
    if (ctx.rose(kPhi2)) {
        rise(ctx);
    } else if (ctx.fell(kPhi2)) {
        fall(ctx);
    }
}

// The control inputs, read at each φ2 edge: an active C1 edge sets its flag (and ends a C2
// handshake); an active C2 edge, while C2 is an input, sets the other.
void Mos6520::sample(EvalContext& ctx) {
    const std::array<bool, 2> c1{ctx.readBit(kCa1), ctx.readBit(kCb1)};
    const std::array<bool, 2> c2{ctx.readBit(kCa2), ctx.readBit(kCb2)};
    for (std::size_t i = 0; i < 2; ++i) {
        Side& s = side_[i];
        if (sampled_) {
            const bool rising = (s.control & kC1Rising) != 0;
            if (c1[i] != s.c1 && c1[i] == rising) {
                s.irq1 = true;
                if (C2Strobed(s.control) && !C2Pulse(s.control)) {
                    s.c2Out = true;  // handshake: C2 back high on the C1 edge
                }
            }
            const bool c2Rising = (s.control & kC2Bit4) != 0;
            if (!C2Output(s.control) && c2[i] != s.c2 && c2[i] == c2Rising) {
                s.irq2 = true;
            }
        }
        s.c1 = c1[i];
        s.c2 = c2[i];
    }
    sampled_ = true;
}

void Mos6520::reset() {
    // RES_N (R6520 p. 9): every register and flag cleared; ports and C2s inputs.
    for (Side& s : side_) {
        s.output = s.direction = s.control = 0;
        s.irq1 = s.irq2 = false;
        s.c2Out = true;
        s.strobe = s.pulseOn = false;
    }
}

bool Mos6520::selected(EvalContext& ctx, bool before) const {
    const auto bit = [&](PinIndex pin) {
        return before ? ctx.readBitBefore(pin) : ctx.readBit(pin);
    };
    return bit(kCs0) && bit(kCs1) && !bit(kCs2N);
}

unsigned Mos6520::registerOf(EvalContext& ctx, bool before) const {
    const auto bit = [&](PinIndex pin) {
        return before ? ctx.readBitBefore(pin) : ctx.readBit(pin);
    };
    return (bit(kRs1) ? 2u : 0u) | (bit(kRs0) ? 1u : 0u);
}

// The φ2 rise: the C lines sampled, a reset applied, a read answered (tDDR); a CB2 write strobe
// pending from the last fall starts, a CB2 pulse from the last rise ends.
void Mos6520::rise(EvalContext& ctx) {
    sample(ctx);
    if (!ctx.readBit(kResN)) {
        reset();
        driveOutputs(ctx);
        return;
    }
    Side& b = side_[1];
    if (b.pulseOn) {
        b.pulseOn = false;
        b.c2Out = true;
    }
    if (b.strobe) {
        b.strobe = false;
        if (C2Strobed(b.control)) {
            b.c2Out = false;
            b.pulseOn = C2Pulse(b.control);
        }
    }
    if (ctx.readBit(kRw) && selected(ctx, false)) {
        const std::uint8_t value = read(ctx, registerOf(ctx, false));
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            drive(ctx, kData[bit], Bit(value, bit), kMos6520DataOut);
        }
        driving_ = true;
    }
    driveOutputs(ctx);
}

std::uint8_t Mos6520::read(EvalContext& ctx, unsigned reg) {
    Side& s = side_[reg >> 1];
    if ((reg & 1) != 0) {  // CRA, CRB: the flags in bits 7 and 6
        const bool irq2 = s.irq2 && !C2Output(s.control);
        return static_cast<std::uint8_t>((s.irq1 ? 0x80 : 0) | (irq2 ? 0x40 : 0) | s.control);
    }
    if ((s.control & kSelectOr) == 0) {
        return s.direction;
    }
    // A port read clears the side's flags; port A starts a CA2 read strobe.
    s.irq1 = s.irq2 = false;
    unsigned value = 0;
    if (reg == 0) {  // PA: the pins, even for output bits
        for (unsigned bit = 0; bit < 8; ++bit) {
            value |= (ctx.readBit(kPortA[bit]) ? 1u : 0u) << bit;
        }
        s.strobe = C2Strobed(s.control);
    } else {  // PB: the output register on output bits, the pins on inputs
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool output = ((s.direction >> bit) & 1) != 0;
            const bool one = output ? ((s.output >> bit) & 1) != 0 : ctx.readBit(kPortB[bit]);
            value |= (one ? 1u : 0u) << bit;
        }
    }
    return static_cast<std::uint8_t>(value);
}

// The φ2 fall: the bus released (tHR), the C lines sampled, a CA2 read strobe or pulse end, a
// write latched.
void Mos6520::fall(EvalContext& ctx) {
    if (driving_) {
        for (const PinIndex pin : kData) {
            drive(ctx, pin, Z, kMos6520DataHold);
        }
        driving_ = false;
    }
    sample(ctx);
    Side& a = side_[0];
    if (a.pulseOn) {
        a.pulseOn = false;
        a.c2Out = true;
    }
    if (a.strobe) {
        a.strobe = false;
        a.c2Out = false;
        a.pulseOn = C2Pulse(a.control);
    }
    if (ctx.readBitBefore(kResN) && !ctx.readBitBefore(kRw) && selected(ctx, true)) {
        unsigned value = 0;
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            value |= (ctx.readBitBefore(kData[bit]) ? 1u : 0u) << bit;
        }
        write(registerOf(ctx, true), static_cast<std::uint8_t>(value));
    }
    driveOutputs(ctx);
}

void Mos6520::write(unsigned reg, std::uint8_t value) {
    Side& s = side_[reg >> 1];
    if ((reg & 1) != 0) {  // CRx: bits 5-0; the flags are read-only
        s.control = value & 0x3F;
        if (C2Output(s.control)) {
            // Manual: the level of bit 3; handshake and pulse start high.
            s.c2Out = (s.control & kC2Bit4) == 0 || (s.control & kC2Bit3) != 0;
            s.pulseOn = s.strobe = false;
        }
        return;
    }
    if ((s.control & kSelectOr) == 0) {
        s.direction = value;
        return;
    }
    s.output = value;
    if (reg == 2) {
        s.strobe = C2Strobed(s.control);  // CB2 low from the next rise
    }
}

void Mos6520::drive(EvalContext& ctx, PinIndex pin, LogicValue value, Time delay) {
    if (driven_[pin] != value) {
        driven_[pin] = value;
        ctx.drive(pin, value, delay);
    }
}

// Every output from the registers; only the Pins whose value changed are driven.
void Mos6520::driveOutputs(EvalContext& ctx) {
    for (unsigned bit = 0; bit < 8; ++bit) {
        const bool outA = (side_[0].direction >> bit) & 1;
        drive(ctx, kPortA[bit], outA && !((side_[0].output >> bit) & 1) ? Zero : One, kMos6520Port);
        const bool outB = (side_[1].direction >> bit) & 1;
        drive(ctx, kPortB[bit], outB ? Bit(side_[1].output, bit) : Z, kMos6520Port);
    }
    const Side& a = side_[0];
    const Side& b = side_[1];
    drive(ctx, kCa2, C2Output(a.control) && !a.c2Out ? Zero : One, kMos6520Control);
    drive(ctx, kCb2, C2Output(b.control) ? (b.c2Out ? One : Zero) : Z, kMos6520Control);
    for (std::size_t i = 0; i < 2; ++i) {
        const Side& s = side_[i];
        const bool irq = (s.irq1 && (s.control & kC1Enable)) ||
                         (s.irq2 && !C2Output(s.control) && (s.control & kC2Bit3));
        drive(ctx, i == 0 ? kIrqAN : kIrqBN, irq ? Zero : One, kMos6520Irq);
    }
}

}  // namespace bw
