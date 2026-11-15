#include "parts/ls365.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> Ls365Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("G1_N"), in("A1"),     out("Y1"),  in("A2"),    out("Y2"), in("A3"),
            out("Y3"),  power("GND"), out("Y4"),  in("A4"),    out("Y5"), in("A5"),
            out("Y6"),  in("A6"),     in("G2_N"), power("VCC")};
}

}  // namespace

Ls365::Ls365(std::string reference) : Component(std::move(reference), Ls365Pins()) {}

void Ls365::powerOn(PowerOnContext& ctx) {
    for (std::size_t i = 0; i < kY.size(); ++i) {
        ctx.setOutput(kY[i], Z);
        driven_[i] = Z;
    }
}

void Ls365::evaluate(EvalContext& ctx) {
    const LogicValue g1 = ctx.read(kG1), g2 = ctx.read(kG2);
    // Off if either enable is high; on if both are low; X otherwise.
    const LogicValue enabled = g1 == One || g2 == One ? Zero : g1 == Zero && g2 == Zero ? One : X;
    for (std::size_t i = 0; i < kY.size(); ++i) {
        LogicValue value = X;
        if (enabled == Zero) {
            value = Z;
        } else if (enabled == One) {
            const LogicValue a = ctx.read(kA[i]);
            value = a == One ? One : a == Zero ? Zero : X;
        }
        Time delay = 0;
        if (value == Z) {
            delay = driven_[i] == One ? delays::kLs365Disable.rise : delays::kLs365Disable.fall;
        } else if (driven_[i] == Z) {
            delay = DelayFor(delays::kLs365Enable, value);
        } else {
            delay = DelayFor(delays::kLs365Data, value);
        }
        driven_[i] = value;
        ctx.drive(kY[i], value, delay);
    }
}

}  // namespace bw
