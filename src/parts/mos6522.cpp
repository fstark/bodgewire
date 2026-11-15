#include "parts/mos6522.hpp"

#include <utility>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using namespace delays;

constexpr LogicValue Bit(unsigned value, unsigned bit) { return (value >> bit) & 1 ? One : Zero; }

// A pull-up Pin: driven 0 or 1 when the chip drives it, else its pull-up's 1.
constexpr LogicValue PullUp(bool drive, bool level) { return drive && !level ? Zero : One; }

}  // namespace

std::vector<PinSpec> Mos6522::Pins() {
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
            pullUp("PB0"),
            pullUp("PB1"),
            pullUp("PB2"),
            pullUp("PB3"),
            pullUp("PB4"),
            pullUp("PB5"),
            pullUp("PB6"),
            pullUp("PB7"),
            pullUp("CB1"),
            pullUp("CB2"),
            power("VCC"),
            PinSpec{"IRQ_N", PinKind::OpenCollector, Family::Mos},
            in("RW"),
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
            in("RS3"),
            in("RS2"),
            in("RS1"),
            in("RS0"),
            pullUp("CA2"),
            in("CA1")};
}

Mos6522::Mos6522(std::string reference) : Component(std::move(reference), Pins()) {}

void Mos6522::powerOn(PowerOnContext& ctx) {
    // A real NMOS chip powers up undefined until RES (VICE: the latches are not 0 at power-up).
    std::array<std::uint8_t, 16> bytes{};
    for (std::uint8_t& byte : bytes) {
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            value |= (ctx.randomBit() ? 1u : 0u) << bit;
        }
        byte = static_cast<std::uint8_t>(value);
    }
    core_.randomise(bytes);
    driving_ = false;
    driven_.fill(Z);
    const Mos6522Core::Outputs o = core_.outputs();
    const auto set = [&](PinIndex pin, LogicValue value) {
        driven_[pin] = value;
        ctx.setOutput(pin, value);
    };
    for (unsigned bit = 0; bit < 8; ++bit) {
        set(kPortA[bit], PullUp((o.paDrive >> bit) & 1, (o.paLevel >> bit) & 1));
        set(kPortB[bit], PullUp((o.pbDrive >> bit) & 1, (o.pbLevel >> bit) & 1));
    }
    set(kCa2, PullUp(o.ca2Drive, o.ca2));
    set(kCb1, PullUp(o.cb1Drive, o.cb1));
    set(kCb2, PullUp(o.cb2Drive, o.cb2));
    set(kIrqN, o.irq ? Zero : One);
}

void Mos6522::evaluate(EvalContext& ctx) {
    if (ctx.rose(kPhi2)) {
        rise(ctx);
    } else if (ctx.fell(kPhi2)) {
        fall(ctx);
    }
}

// The φ2 rise: the inputs sampled, a reset applied, a read answered.
void Mos6522::rise(EvalContext& ctx) {
    if (!ctx.readBit(kResN)) {
        core_.reset();
        driveOutputs(ctx);
        return;
    }
    Mos6522Core::Inputs in;
    unsigned pa = 0, pb = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        pa |= (ctx.readBit(kPortA[bit]) ? 1u : 0u) << bit;
        pb |= (ctx.readBit(kPortB[bit]) ? 1u : 0u) << bit;
    }
    in.pa = static_cast<std::uint8_t>(pa);
    in.pb = static_cast<std::uint8_t>(pb);
    in.ca1 = ctx.readBit(kCa1);
    in.ca2 = ctx.readBit(kCa2);
    in.cb1 = ctx.readBit(kCb1);
    in.cb2 = ctx.readBit(kCb2);
    core_.rise(in);
    if (ctx.readBit(kRw) && ctx.readBit(kCs1) && !ctx.readBit(kCs2N)) {
        unsigned reg = 0;
        for (unsigned bit = 0; bit < kRs.size(); ++bit) {
            reg |= (ctx.readBit(kRs[bit]) ? 1u : 0u) << bit;
        }
        const std::uint8_t value = core_.read(reg);
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            drive(ctx, kData[bit], Bit(value, bit), kMos6522DataOut);
        }
        driving_ = true;
    }
    driveOutputs(ctx);
}

// The φ2 fall: the bus released, a write latched, then the clocked state (timers, shifts).
void Mos6522::fall(EvalContext& ctx) {
    if (driving_) {
        for (const PinIndex pin : kData) {
            drive(ctx, pin, Z, kMos6522DataHold);
        }
        driving_ = false;
    }
    if (!ctx.readBitBefore(kResN)) {
        return;
    }
    if (!ctx.readBitBefore(kRw) && ctx.readBitBefore(kCs1) && !ctx.readBitBefore(kCs2N)) {
        unsigned reg = 0, value = 0;
        for (unsigned bit = 0; bit < kRs.size(); ++bit) {
            reg |= (ctx.readBitBefore(kRs[bit]) ? 1u : 0u) << bit;
        }
        for (unsigned bit = 0; bit < kData.size(); ++bit) {
            value |= (ctx.readBitBefore(kData[bit]) ? 1u : 0u) << bit;
        }
        core_.write(reg, static_cast<std::uint8_t>(value));
    }
    core_.fall();
    driveOutputs(ctx);
}

void Mos6522::drive(EvalContext& ctx, PinIndex pin, LogicValue value, Time delay) {
    if (driven_[pin] != value) {
        driven_[pin] = value;
        ctx.drive(pin, value, delay);
    }
}

// Every output from the core; only the Pins whose value changed are driven.
void Mos6522::driveOutputs(EvalContext& ctx) {
    const Mos6522Core::Outputs o = core_.outputs();
    for (unsigned bit = 0; bit < 8; ++bit) {
        drive(ctx, kPortA[bit], PullUp((o.paDrive >> bit) & 1, (o.paLevel >> bit) & 1),
              kMos6522Port);
        const Time delay = bit == 7 && o.pb7Timer ? kMos6522Shift : kMos6522Port;
        drive(ctx, kPortB[bit], PullUp((o.pbDrive >> bit) & 1, (o.pbLevel >> bit) & 1), delay);
    }
    drive(ctx, kCa2, PullUp(o.ca2Drive, o.ca2), kMos6522Control);
    drive(ctx, kCb1, PullUp(o.cb1Drive, o.cb1), kMos6522Shift);
    drive(ctx, kCb2, PullUp(o.cb2Drive, o.cb2), o.cb2Shift ? kMos6522Shift : kMos6522Control);
    drive(ctx, kIrqN, o.irq ? Zero : One, kMos6522Irq);
}

}  // namespace bw
