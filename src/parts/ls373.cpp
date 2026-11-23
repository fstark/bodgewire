#include "parts/ls373.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> Ls373Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("OE_N"), out("1Q"), in("1D"),     in("2D"), out("2Q"), out("3Q"),   in("3D"),
            in("4D"),   out("4Q"), power("GND"), in("LE"), out("5Q"), in("5D"),    in("6D"),
            out("6Q"),  out("7Q"), in("7D"),     in("8D"), out("8Q"), power("VCC")};
}

LogicValue Known(LogicValue v) { return v == Zero || v == One ? v : X; }

}  // namespace

Ls373::Ls373(std::string reference) : Component(std::move(reference), Ls373Pins()) {}

void Ls373::powerOn(PowerOnContext& ctx) {
    for (std::size_t bit = 0; bit < kQ.size(); ++bit) {
        latch_[bit] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kQ[bit], Z);  // the first evaluation turns them on per OE_N
        driven_[bit] = Z;
    }
}

void Ls373::evaluate(EvalContext& ctx) {
    const LogicValue le = ctx.read(kLe);
    const bool latchRose = ctx.rose(kLe);
    const LogicValue oe = ctx.read(kOe);
    for (std::size_t bit = 0; bit < kQ.size(); ++bit) {
        LogicValue& q = latch_[bit];
        if (le == One) {
            q = Known(ctx.read(kD[bit]));
        } else if (le == Zero) {
            if (ctx.fell(kLe)) {
                q = Known(ctx.readBefore(kD[bit]));  // held: D from just before the fall
            }
        } else if (Known(ctx.read(kD[bit])) != q) {
            q = X;
        }
        const LogicValue value = oe == One ? Z : oe == Zero ? q : X;
        Time delay = 0;
        if (value == Z) {
            delay = driven_[bit] == One ? delays::kLs373Disable.rise : delays::kLs373Disable.fall;
        } else if (driven_[bit] == Z) {
            delay = DelayFor(delays::kLs373Enable, value);
        } else if (latchRose) {
            delay = DelayFor(delays::kLs373Latch, value);
        } else {
            delay = DelayFor(delays::kLs373Data, value);
        }
        if (value != driven_[bit]) {
            driven_[bit] = value;
            ctx.drive(kQ[bit], value, delay);
        }
    }
}

}  // namespace bw
