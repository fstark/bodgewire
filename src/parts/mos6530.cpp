#include "parts/mos6530.hpp"

#include <utility>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;
using namespace delays;

constexpr std::array<Mos6530Mask, 2> s_masks{{{"002", 0b1111, 0b1101}, {"003", 0b1110, 0b1100}}};

constexpr std::array<std::string_view, 2> s_memories{"rom", "ram"};

constexpr std::array<unsigned, 4> kTimerShift{0, 3, 6, 10};  // ÷1, ÷8, ÷64, ÷1024 by A1 A0

constexpr LogicValue Bit(unsigned value, unsigned bit) { return (value >> bit) & 1 ? One : Zero; }

}  // namespace

const Mos6530Mask* FindMos6530Mask(std::string_view name) {
    for (const Mos6530Mask& mask : s_masks) {
        if (mask.name == name) {
            return &mask;
        }
    }
    return nullptr;
}

std::vector<PinSpec> Mos6530::Pins() {
    // NMOS: every signal pin MOS (X read as a random bit). Only PHI2 wakes the chip.
    const auto in = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Mos, false, true};  // sampled
    };
    const auto data = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Mos, false, true};
    };
    const auto port = [](const char* name) {
        return PinSpec{name, PinKind::PullUp, Family::Mos, false, true};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {power("GND"), port("PA0"),  PinSpec{"PHI2", PinKind::Input, Family::Mos},
            in("RS0"),    in("A9"),     in("A8"),
            in("A7"),     in("A6"),     in("RW"),
            in("A5"),     in("A4"),     in("A3"),
            in("A2"),     in("A1"),     in("A0"),
            in("RES_N"),  port("PB7"),  in("CS1"),
            port("PB5"),  power("VCC"), port("PB4"),
            port("PB3"),  port("PB2"),  port("PB1"),
            port("PB0"),  data("D7"),   data("D6"),
            data("D5"),   data("D4"),   data("D3"),
            data("D2"),   data("D1"),   data("D0"),
            port("PA7"),  port("PA6"),  port("PA5"),
            port("PA4"),  port("PA3"),  port("PA2"),
            port("PA1")};
}

Mos6530::Mos6530(std::string reference, const Mos6530Mask& mask, std::vector<std::uint8_t> rom)
    : Component(std::move(reference), Pins()), mask_(mask), rom_(std::move(rom)) {
    Check(rom_.size() == 1024, "6530: the ROM holds 1024 words");
}

std::span<const std::string_view> Mos6530::memories() const { return s_memories; }

std::size_t Mos6530::wordCount(std::size_t memory) const {
    Check(memory < s_memories.size(), "6530: no such memory");
    return memory == 0 ? rom_.size() : ram_.size();
}

std::optional<unsigned> Mos6530::dataBit(PinIndex pin) const {
    for (unsigned bit = 0; bit < kData.size(); ++bit) {
        if (kData[bit] == pin) {
            return bit;
        }
    }
    return std::nullopt;
}

void Mos6530::stick(std::size_t memory, std::size_t word, unsigned bit, bool one) {
    Check(memory < s_memories.size(), "6530: no such memory");
    if (memory == 0) {
        rom_.stick(word, bit, one);
    } else {
        ram_.stick(word, bit, one);
    }
}

void Mos6530::powerOn(PowerOnContext& ctx) {
    // A real NMOS chip powers up undefined until RES; this one with random registers.
    ram_.powerOn(ctx);
    const auto byte = [&ctx] {
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            value |= (ctx.randomBit() ? 1u : 0u) << bit;
        }
        return static_cast<std::uint8_t>(value);
    };
    port_[0] = byte();
    direction_[0] = byte();
    port_[1] = byte();
    direction_[1] = byte();
    timerCount_ = byte();
    timerShift_ = 10;
    timerStart_ = 0;
    interruptAt_ = std::uint64_t{timerCount_} << timerShift_;
    flagPending_ = true;
    flag_ = irqEnabled_ = driving_ = false;
    pulse_ = 0;
    for (std::size_t p = 0; p < 2; ++p) {
        // A Pin is 0 where its direction bit is 1 and its data bit 0; else its pull-up's 1.
        driven_[p] = static_cast<std::uint8_t>(~(direction_[p] & ~port_[p]));
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        ctx.setOutput(kPortA[bit], Bit(driven_[0], bit));
        if (kPortB[bit] != kNoPin) {
            ctx.setOutput(kPortB[bit], Bit(driven_[1], bit));
        }
    }
}

void Mos6530::evaluate(EvalContext& ctx) {
    if (ctx.rose(kPhi2)) {
        rise(ctx);
    } else if (ctx.fell(kPhi2)) {
        fall(ctx);
    }
}

Mos6530::Access Mos6530::decode(EvalContext& ctx, bool before) {
    const auto bit = [&](PinIndex pin) {
        return before ? ctx.readBitBefore(pin) : ctx.readBit(pin);
    };
    unsigned address = 0;
    for (unsigned i = 0; i < kAddress.size(); ++i) {
        address |= (bit(kAddress[i]) ? 1u : 0u) << i;
    }
    const bool rs0 = bit(kRs0);
    const bool selected = rs0 && !bit(kCs1);
    const unsigned high = address >> 6;  // A9-A6
    return {!rs0, selected && high == mask_.ram, selected && high == mask_.io, address};
}

std::uint8_t Mos6530::timerValue() const {
    if (pulse_ < interruptAt_) {
        const auto counted = (pulse_ - timerStart_) >> timerShift_;
        return static_cast<std::uint8_t>(timerCount_ - 1 - counted);
    }
    return static_cast<std::uint8_t>(0xFF - ((pulse_ - interruptAt_) & 0xFF));
}

// The φ2 rise: a read is answered (TCDR), a reset applied; the port Pins are read here (TPCR).
void Mos6530::rise(EvalContext& ctx) {
    ++pulse_;
    if (!ctx.readBit(kResN)) {
        port_ = {};
        direction_ = {};
        irqEnabled_ = false;
        drivePorts(ctx, kMos6530Port);
        return;
    }
    const Access access = decode(ctx, false);
    if (!ctx.readBit(kRw) || !(access.rom || access.ram || access.io)) {
        return;
    }
    std::uint8_t value = 0;
    if (access.rom) {
        value = rom_[access.address];
    } else if (access.ram) {
        value = ram_[access.address & 0x3F];
    } else {
        value = readRegister(ctx, access.address);
    }
    for (unsigned bit = 0; bit < kData.size(); ++bit) {
        ctx.drive(kData[bit], Bit(value, bit), kMos6530DataOut);
    }
    driving_ = true;
}

std::uint8_t Mos6530::readRegister(EvalContext& ctx, unsigned address) {
    if ((address & 0x4) == 0) {  // A2 low: the ports
        const unsigned which = (address >> 1) & 1;
        if ((address & 1) != 0) {
            return direction_[which];
        }
        const auto& pins = which == 0 ? kPortA : kPortB;
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool output = ((direction_[which] >> bit) & 1) != 0 || pins[bit] == kNoPin;
            const bool one = output ? ((port_[which] >> bit) & 1) != 0 : ctx.readBit(pins[bit]);
            value |= (one ? 1u : 0u) << bit;
        }
        return static_cast<std::uint8_t>(value);
    }
    if ((address & 1) != 0) {  // the interrupt flag on D7
        return flag_ ? 0x80 : 0x00;
    }
    // The timer; A3 enables the interrupt. A read after the flag's own pulse clears it.
    const std::uint8_t value = timerValue();
    irqEnabled_ = (address & 0x8) != 0;
    if (flag_ && pulse_ > interruptAt_) {
        flag_ = false;
    }
    drivePorts(ctx, kMos6530Irq);
    return value;
}

// The φ2 fall: the bus released, a write latched, the flag set before its pulse (TIC).
void Mos6530::fall(EvalContext& ctx) {
    if (driving_) {
        for (const PinIndex pin : kData) {
            ctx.drive(pin, Z, kMos6530DataHold);
        }
        driving_ = false;
    }
    if (ctx.readBitBefore(kResN) && !ctx.readBitBefore(kRw)) {
        const Access access = decode(ctx, true);
        if (access.ram || access.io) {
            unsigned value = 0;
            for (unsigned bit = 0; bit < kData.size(); ++bit) {
                value |= (ctx.readBitBefore(kData[bit]) ? 1u : 0u) << bit;
            }
            if (access.ram) {
                ram_.store(access.address & 0x3F, value);
            } else {
                writeRegister(access.address, static_cast<std::uint8_t>(value));
                drivePorts(ctx, (access.address & 0x4) == 0 ? kMos6530Port : kMos6530Irq);
            }
        }
    }
    if (flagPending_ && pulse_ + 1 >= interruptAt_) {
        flagPending_ = false;
        flag_ = true;
        drivePorts(ctx, kMos6530Irq);
    }
}

void Mos6530::writeRegister(unsigned address, std::uint8_t value) {
    if ((address & 0x4) == 0) {  // A2 low: data A, direction A, data B, direction B
        const unsigned which = (address >> 1) & 1;
        ((address & 1) != 0 ? direction_ : port_)[which] = value;
        return;
    }
    timerCount_ = value;
    timerShift_ = kTimerShift[address & 0x3];
    timerStart_ = pulse_;
    interruptAt_ = pulse_ + (std::uint64_t{value} << timerShift_);
    flagPending_ = true;
    flag_ = false;
    irqEnabled_ = (address & 0x8) != 0;
}

void Mos6530::drivePorts(EvalContext& ctx, Time delay) {
    for (std::size_t p = 0; p < 2; ++p) {
        unsigned value = ~(direction_[p] & ~port_[p]) & 0xFFu;
        if (p == 1 && irqEnabled_ && flag_) {
            value &= 0x7Fu;  // PB7 low: the interrupt
        }
        const unsigned changed = value ^ driven_[p];
        if (changed == 0) {
            continue;
        }
        driven_[p] = static_cast<std::uint8_t>(value);
        const auto& pins = p == 0 ? kPortA : kPortB;
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (((changed >> bit) & 1) != 0 && pins[bit] != kNoPin) {
                ctx.drive(pins[bit], Bit(value, bit), delay);
            }
        }
    }
}

}  // namespace bw
