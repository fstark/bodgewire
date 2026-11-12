#include "parts/shift_register_2524.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> ShiftRegister2524Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Mos}; };
    const auto clock = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Mos, true};
    };
    return {clock("PHI2"), {"OUT", PinKind::Output, Family::Mos},
            in("READ"),    {"VDD", PinKind::NoConnect, Family::Mos},
            in("WRITE"),   in("IN"),
            clock("PHI1"), {"VCC", PinKind::Power, Family::Ttl}};
}

}  // namespace

ShiftRegister2524::ShiftRegister2524(std::string reference)
    : Component(std::move(reference), ShiftRegister2524Pins()) {}

void ShiftRegister2524::powerOn(PowerOnContext& ctx) {
    for (std::size_t i = 0; i < kBits; ++i) {
        bits_[i] = ctx.randomBit();
    }
    head_ = 0;
    out_ = bits_[0];
    lastPhi1_ = lastPhi2_ = lastDecay_ = 0;
    ctx.setOutput(kOut, out_ ? One : Zero);
}

void ShiftRegister2524::evaluate(EvalContext& ctx) {
    const bool phi1 = ctx.rose(kPhi1);
    const bool phi2 = ctx.rose(kPhi2);
    if (phi1 || phi2) {
        const Time now = ctx.now();
        if (now - std::max(std::min(lastPhi1_, lastPhi2_), lastDecay_) > delays::k2524Hold) {
            for (std::size_t i = 0; i < kBits; ++i) {
                bits_[i] = ctx.randomBit();
            }
            lastDecay_ = now;
        }
        if (phi1) {
            lastPhi1_ = now;
        }
        if (phi2) {
            lastPhi2_ = now;
        }
    }
    if (phi2 && ctx.readBitBefore(kWrite)) {
        bits_[head_] = ctx.readBitBefore(kIn);
    }
    if (phi1) {
        head_ = (head_ + 1) % kBits;
        out_ = bits_[head_];
    }
    ctx.drive(kOut, ctx.readBit(kRead) && out_ ? One : Zero, delays::k2524ClockToOutput);
}

}  // namespace bw
