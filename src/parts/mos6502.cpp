#include "parts/mos6502.hpp"

#include <cstdint>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using namespace delays;

std::vector<PinSpec> Mos6502Pins() {
    // NMOS: every signal pin MOS (bus charge, X read as a random bit), NoConnect included.
    const auto in = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Mos, false, true};  // sampled
    };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Mos}; };
    const auto io = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Mos, false, true};  // sampled
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto nc = [](const char* name) { return PinSpec{name, PinKind::NoConnect, Family::Mos}; };
    return {power("GND"),
            in("RDY"),
            out("PHI1"),
            in("IRQ_N"),
            nc("NC1"),
            in("NMI_N"),
            out("SYNC"),
            power("VCC"),
            out("A0"),
            out("A1"),
            out("A2"),
            out("A3"),
            out("A4"),
            out("A5"),
            out("A6"),
            out("A7"),
            out("A8"),
            out("A9"),
            out("A10"),
            out("A11"),
            power("GND2"),
            out("A12"),
            out("A13"),
            out("A14"),
            out("A15"),
            io("D7"),
            io("D6"),
            io("D5"),
            io("D4"),
            io("D3"),
            io("D2"),
            io("D1"),
            io("D0"),
            out("RW"),
            nc("NC2"),
            nc("NC3"),
            PinSpec{"PHI0", PinKind::Input, Family::Mos},
            in("SO"),
            out("PHI2"),
            in("RES_N")};
}

constexpr LogicValue Bit(unsigned value, unsigned bit) { return (value >> bit) & 1 ? One : Zero; }

}  // namespace

Mos6502::Mos6502(std::string reference) : Component(std::move(reference), Mos6502Pins()) {}

void Mos6502::powerOn(PowerOnContext& ctx) {
    // A real NMOS 6502 powers up in an undefined state; this one at an opcode fetch somewhere.
    const auto byte = [&ctx] {
        unsigned value = 0;
        for (int i = 0; i < 8; ++i) {
            value = value << 1 | (ctx.randomBit() ? 1u : 0u);
        }
        return static_cast<std::uint8_t>(value);
    };
    Mos6502Registers r;
    r.pc = static_cast<std::uint16_t>(byte() << 8 | byte());
    r.a = byte();
    r.x = byte();
    r.y = byte();
    r.s = byte();
    r.p = static_cast<std::uint8_t>(byte() | 0x20);
    core_.start(r);
    so_ = true;
    for (unsigned bit = 0; bit < kAddress.size(); ++bit) {
        ctx.setOutput(kAddress[bit], Bit(r.pc, bit));
    }
    ctx.setOutput(kRw, One);
    ctx.setOutput(kSync, One);
    ctx.setOutput(kPhi1, One);
    ctx.setOutput(kPhi2, Zero);
}

void Mos6502::evaluate(EvalContext& ctx) {
    if (ctx.rose(kPhi0)) {
        phase2(ctx);
    } else if (ctx.fell(kPhi0)) {
        endCycle(ctx);
    }
}

// PHI0 rises: phase 2. SO is sampled on the trailing edge of phase 1 (data sheet p. 7).
void Mos6502::phase2(EvalContext& ctx) {
    ctx.drive(kPhi1, Zero, DelayFor(kMos6502Phi1, Zero));
    ctx.drive(kPhi2, One, DelayFor(kMos6502Phi2, One));
    const bool so = ctx.readBitBefore(kSo);
    if (so_ && !so) {
        core_.setOverflow();
    }
    so_ = so;
    const Mos6502Bus& bus = core_.bus();
    if (!bus.read) {
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            ctx.drive(kData[bit], Bit(bus.data, bit), kMos6502DataOut);
        }
    }
}

// PHI0 falls: the cycle ends; the next one's address, RW and SYNC are driven.
void Mos6502::endCycle(EvalContext& ctx) {
    ctx.drive(kPhi1, One, DelayFor(kMos6502Phi1, One));
    ctx.drive(kPhi2, Zero, DelayFor(kMos6502Phi2, Zero));
    const bool reset = !ctx.readBitBefore(kResN);
    core_.setReset(reset);
    core_.setNmi(!ctx.readBitBefore(kNmiN));
    core_.setIrq(!ctx.readBitBefore(kIrqN));
    const Mos6502Bus& bus = core_.bus();
    if (bus.read && !reset && !ctx.readBitBefore(kRdy)) {
        return;  // RDY low: the read cycle runs again; ignored on a write (NMOS)
    }
    unsigned data = 0;
    if (bus.read) {
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            data |= (ctx.readBitBefore(kData[bit]) ? 1u : 0u) << bit;
        }
    } else {
        for (const PinIndex pin : kData) {
            ctx.drive(pin, Z, kMos6502DataHold);
        }
    }
    core_.step(static_cast<std::uint8_t>(data));
    const Mos6502Bus& next = core_.bus();
    for (unsigned bit = 0; bit < kAddress.size(); ++bit) {
        ctx.drive(kAddress[bit], Bit(next.address, bit), kMos6502Address);
    }
    ctx.drive(kRw, next.read ? One : Zero, kMos6502ReadWrite);
    ctx.drive(kSync, next.sync ? One : Zero, kMos6502Sync);
}

}  // namespace bw
