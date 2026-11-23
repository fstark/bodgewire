#include "parts/z80.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using namespace delays;
using Kind = Z80CycleKind;

std::vector<PinSpec> Z80Pins() {
    // NMOS: every signal pin MOS (bus charge, X read as a random bit); power pins TTL.
    const auto in = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Mos, false, true};  // sampled
    };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Mos}; };
    const auto io = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Mos, false, true};  // sampled
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {out("A11"),    out("A12"),     out("A13"),
            out("A14"),    out("A15"),     PinSpec{"CLK", PinKind::Input, Family::Mos},
            io("D4"),      io("D3"),       io("D5"),
            io("D6"),      power("VCC"),   io("D2"),
            io("D7"),      io("D0"),       io("D1"),
            in("INT_N"),   in("NMI_N"),    out("HALT_N"),
            out("MREQ_N"), out("IORQ_N"),  out("RD_N"),
            out("WR_N"),   out("BUSAK_N"), in("WAIT_N"),
            in("BUSRQ_N"), in("RESET_N"),  out("M1_N"),
            out("RFSH_N"), power("GND"),   out("A0"),
            out("A1"),     out("A2"),      out("A3"),
            out("A4"),     out("A5"),      out("A6"),
            out("A7"),     out("A8"),      out("A9"),
            out("A10")};
}

constexpr LogicValue Bit(unsigned value, unsigned bit) { return (value >> bit) & 1 ? One : Zero; }

constexpr bool IsM1(Kind kind) { return kind == Kind::Fetch || kind == Kind::Acknowledge; }

}  // namespace

Z80::Z80(std::string reference) : Component(std::move(reference), Z80Pins()) {}

void Z80::powerOn(PowerOnContext& ctx) {
    // A real NMOS Z80 powers up in an undefined state; this one at an opcode fetch somewhere.
    const auto bits = [&ctx](int n) {
        unsigned value = 0;
        for (int i = 0; i < n; ++i) {
            value = value << 1 | (ctx.randomBit() ? 1u : 0u);
        }
        return value;
    };
    const auto byte = [&] { return static_cast<std::uint8_t>(bits(8)); };
    const auto word = [&] { return static_cast<std::uint16_t>(bits(16)); };
    Z80Registers r;
    r.pc = word();
    r.sp = word();
    r.ix = word();
    r.iy = word();
    r.wz = word();
    r.a = byte();
    r.f = byte();
    r.b = byte();
    r.c = byte();
    r.d = byte();
    r.e = byte();
    r.h = byte();
    r.l = byte();
    r.i = byte();
    r.r = byte();
    r.af2 = word();
    r.bc2 = word();
    r.de2 = word();
    r.hl2 = word();
    r.im = static_cast<std::uint8_t>(bits(2) % 3);
    r.iff1 = r.iff2 = ctx.randomBit();
    core_.start(r);
    phase_ = Phase::Next;
    mode_ = Mode::Running;
    internalLeft_ = 0;
    latched_ = 0;
    waitAsked_ = busRequested_ = false;
    dataDriven_ = floating_ = refreshing_ = haltLow_ = false;
    for (unsigned bit = 0; bit < kAddress.size(); ++bit) {
        ctx.setOutput(kAddress[bit], Bit(r.pc, bit));
    }
    for (const PinIndex pin : {kHaltN, kMreqN, kIorqN, kRdN, kWrN, kBusakN, kM1N, kRfshN}) {
        ctx.setOutput(pin, One);
    }
}

void Z80::evaluate(EvalContext& ctx) {
    if (ctx.rose(kClk)) {
        rise(ctx);
    } else if (ctx.fell(kClk)) {
        fall(ctx);
    }
}

void Z80::driveAddress(EvalContext& ctx, std::uint16_t address) {
    for (unsigned bit = 0; bit < kAddress.size(); ++bit) {
        ctx.drive(kAddress[bit], Bit(address, bit), kZ80Address);
    }
}

void Z80::driveData(EvalContext& ctx, std::uint8_t data) {
    for (unsigned bit = 0; bit < kData.size(); ++bit) {
        ctx.drive(kData[bit], Bit(data, bit), kZ80Data);
    }
    dataDriven_ = true;
}

void Z80::releaseData(EvalContext& ctx, Time delay) {
    if (!dataDriven_) {
        return;
    }
    for (const PinIndex pin : kData) {
        ctx.drive(pin, Z, delay);
    }
    dataDriven_ = false;
}

// Bus grant: A, D and the memory and I/O strobes float; M1_N, RFSH_N, HALT_N stay driven.
void Z80::floatBus(EvalContext& ctx) {
    for (const PinIndex pin : kAddress) {
        ctx.drive(pin, Z, kZ80Float);
    }
    releaseData(ctx, kZ80Float);
    for (const PinIndex pin : {kMreqN, kIorqN, kRdN, kWrN}) {
        ctx.drive(pin, Z, kZ80Float);
    }
    floating_ = true;
}

std::uint8_t Z80::readData(EvalContext& ctx) {
    unsigned value = 0;
    for (unsigned bit = 0; bit < kData.size(); ++bit) {
        value |= (ctx.readBitBefore(kData[bit]) ? 1u : 0u) << bit;
    }
    return static_cast<std::uint8_t>(value);
}

Z80::Phase Z80::following() const {
    const Kind kind = cycle_.kind;
    switch (phase_) {
    case Phase::T1:
        return Phase::T2;
    case Phase::T2:
        if (kind == Kind::In || kind == Kind::Out || kind == Kind::Acknowledge) {
            return Phase::Wa1;
        }
        return waitAsked_ ? Phase::Tw : Phase::T3;
    case Phase::Wa1:
        if (kind == Kind::Acknowledge) {
            return Phase::Wa2;
        }
        return waitAsked_ ? Phase::Tw : Phase::T3;
    case Phase::Wa2:
    case Phase::Tw:
        return waitAsked_ ? Phase::Tw : Phase::T3;
    case Phase::T3:
        return IsM1(kind) ? Phase::T4 : Phase::Next;
    case Phase::Internal:
        return internalLeft_ > 1 ? Phase::Internal : Phase::Next;
    default:
        return Phase::Next;
    }
}

bool Z80::lastPhase() const {
    switch (phase_) {
    case Phase::T4:
        return true;
    case Phase::T3:
        return !IsM1(cycle_.kind);
    case Phase::Internal:
        return internalLeft_ == 1;
    default:
        return false;
    }
}

// T1's rising edge, or an Internal cycle's first.
void Z80::startCycle(EvalContext& ctx) {
    cycle_ = core_.cycle();
    waitAsked_ = false;
    releaseData(ctx, kZ80Data);
    if (refreshing_) {
        ctx.drive(kRfshN, One, kZ80Rfsh);
        refreshing_ = false;
    }
    if (haltLow_ && !(cycle_.kind == Kind::Fetch && cycle_.halted)) {
        ctx.drive(kHaltN, One, kZ80Halt);
        haltLow_ = false;
    }
    if (cycle_.kind == Kind::Internal) {
        phase_ = Phase::Internal;
        internalLeft_ = cycle_.length;
        return;
    }
    phase_ = Phase::T1;
    if (floating_) {  // the bus given back
        for (const PinIndex pin : {kMreqN, kIorqN, kRdN, kWrN}) {
            ctx.drive(pin, One, kZ80Float);
        }
        floating_ = false;
    }
    driveAddress(ctx, cycle_.address);
    if (IsM1(cycle_.kind)) {
        ctx.drive(kM1N, Zero, kZ80M1);
    }
}

// The rising edge of a cycle's last T-state: INT and BUSRQ sampled (UM0080: "on the rising edge
// of the last clock"); a Fetch, Acknowledge or Internal ends here, the others at T3's fall.
void Z80::lastRise(EvalContext& ctx) {
    core_.setInt(!ctx.readBitBefore(kIntN));
    busRequested_ = !ctx.readBitBefore(kBusrqN);
    if (IsM1(cycle_.kind) || cycle_.kind == Kind::Internal) {
        core_.step(latched_);
    }
}

void Z80::rise(EvalContext& ctx) {
    core_.setNmi(!ctx.readBitBefore(kNmiN));  // at every edge: a short pulse is kept
    if (!ctx.readBitBefore(kResetN)) {
        if (mode_ != Mode::Reset) {
            // UM0080: the address and data buses float, the control outputs go inactive.
            for (const PinIndex pin : kAddress) {
                ctx.drive(pin, Z, kZ80Float);
            }
            releaseData(ctx, kZ80Float);
            for (const PinIndex pin : {kMreqN, kIorqN, kRdN, kWrN, kM1N, kRfshN, kHaltN, kBusakN}) {
                ctx.drive(pin, One, kZ80Float);
            }
            mode_ = Mode::Reset;
            phase_ = Phase::Next;
            floating_ = refreshing_ = haltLow_ = busRequested_ = false;
        }
        return;
    }
    if (mode_ == Mode::Reset) {
        core_.reset();  // the next rising edge is T1 of the fetch at $0000
        mode_ = Mode::Running;
        return;
    }
    if (mode_ == Mode::Granted) {
        if (ctx.readBitBefore(kBusrqN)) {
            ctx.drive(kBusakN, One, kZ80Busak);
            mode_ = Mode::Running;
            busRequested_ = false;
        }
        return;
    }
    const Phase next = phase_ == Phase::Next ? Phase::Next : following();
    if (next == Phase::Next) {
        if (busRequested_) {
            mode_ = Mode::Granted;
            phase_ = Phase::Next;
            ctx.drive(kBusakN, Zero, kZ80Busak);
            if (refreshing_) {
                ctx.drive(kRfshN, One, kZ80Rfsh);
                refreshing_ = false;
            }
            floatBus(ctx);
            return;
        }
        startCycle(ctx);
    } else {
        if (next == Phase::Internal) {
            --internalLeft_;
        }
        phase_ = next;
    }
    const Kind kind = cycle_.kind;
    if (phase_ == Phase::T2 && (kind == Kind::In || kind == Kind::Out)) {
        ctx.drive(kIorqN, Zero, kZ80Iorq);
        if (kind == Kind::In) {
            ctx.drive(kRdN, Zero, kZ80Rd);
        } else {
            ctx.drive(kWrN, Zero, kZ80Wr);
        }
    } else if (phase_ == Phase::T3 && IsM1(kind)) {
        latched_ = readData(ctx);
        if (kind == Kind::Fetch) {
            ctx.drive(kMreqN, One, kZ80Mreq);
            ctx.drive(kRdN, One, kZ80Rd);
        } else {
            ctx.drive(kIorqN, One, kZ80Iorq);
        }
        ctx.drive(kM1N, One, kZ80M1);
        driveAddress(ctx, cycle_.refresh);
        ctx.drive(kRfshN, Zero, kZ80Rfsh);
        refreshing_ = true;
    }
    if (lastPhase()) {
        lastRise(ctx);
    }
}

void Z80::fall(EvalContext& ctx) {
    if (mode_ != Mode::Running || phase_ == Phase::Next) {
        return;
    }
    const auto sampleWait = [&] { waitAsked_ = !ctx.readBitBefore(kWaitN); };
    switch (cycle_.kind) {
    case Kind::Fetch:
        if (phase_ == Phase::T1) {
            ctx.drive(kMreqN, Zero, kZ80Mreq);
            ctx.drive(kRdN, Zero, kZ80Rd);
        } else if (phase_ == Phase::T2 || phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            ctx.drive(kMreqN, Zero, kZ80Mreq);
        } else if (phase_ == Phase::T4) {
            ctx.drive(kMreqN, One, kZ80Mreq);
            if (core_.halted() && !haltLow_) {
                ctx.drive(kHaltN, Zero, kZ80Halt);
                haltLow_ = true;
            }
        }
        break;
    case Kind::Read:
        if (phase_ == Phase::T1) {
            ctx.drive(kMreqN, Zero, kZ80Mreq);
            ctx.drive(kRdN, Zero, kZ80Rd);
        } else if (phase_ == Phase::T2 || phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            const std::uint8_t data = readData(ctx);
            ctx.drive(kMreqN, One, kZ80Mreq);
            ctx.drive(kRdN, One, kZ80Rd);
            core_.step(data);
        }
        break;
    case Kind::Write:
        if (phase_ == Phase::T1) {
            ctx.drive(kMreqN, Zero, kZ80Mreq);
            driveData(ctx, cycle_.data);
        } else if (phase_ == Phase::T2) {
            ctx.drive(kWrN, Zero, kZ80Wr);
            sampleWait();
        } else if (phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            ctx.drive(kMreqN, One, kZ80Mreq);
            ctx.drive(kWrN, One, kZ80Wr);
            core_.step(0);
        }
        break;
    case Kind::In:
        if (phase_ == Phase::Wa1 || phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            const std::uint8_t data = readData(ctx);
            ctx.drive(kIorqN, One, kZ80Iorq);
            ctx.drive(kRdN, One, kZ80Rd);
            core_.step(data);
        }
        break;
    case Kind::Out:
        if (phase_ == Phase::T1) {
            driveData(ctx, cycle_.data);
        } else if (phase_ == Phase::Wa1 || phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            ctx.drive(kIorqN, One, kZ80Iorq);
            ctx.drive(kWrN, One, kZ80Wr);
            core_.step(0);
        }
        break;
    case Kind::Acknowledge:
        if (phase_ == Phase::Wa1) {
            ctx.drive(kIorqN, Zero, kZ80Iorq);
        } else if (phase_ == Phase::Wa2 || phase_ == Phase::Tw) {
            sampleWait();
        } else if (phase_ == Phase::T3) {
            ctx.drive(kMreqN, Zero, kZ80Mreq);
        } else if (phase_ == Phase::T4) {
            ctx.drive(kMreqN, One, kZ80Mreq);
        }
        break;
    case Kind::Internal:
        break;
    }
}

}  // namespace bw
