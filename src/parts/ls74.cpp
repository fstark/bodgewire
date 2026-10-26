#include "parts/ls74.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct FlipFlopPins {
    PinIndex clr, d, clk, pre, q, qn;
};

constexpr std::array<FlipFlopPins, 2> kUnits{{{0, 1, 2, 3, 4, 5}, {12, 11, 10, 9, 8, 7}}};

std::vector<PinSpec> Ls74Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("1CLR_N"), in("1D"),     in("1CLK"),   in("1PRE_N"), out("1Q"),
            out("1Q_N"),  power("GND"), out("2Q_N"),  out("2Q"),    in("2PRE_N"),
            in("2CLK"),   in("2D"),     in("2CLR_N"), power("VCC")};
}

}  // namespace

Ls74::Ls74(std::string reference) : Component(std::move(reference), Ls74Pins()) {}

void Ls74::powerOn(PowerOnContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        state_[u] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kUnits[u].q, state_[u]);
        ctx.setOutput(kUnits[u].qn, Not(state_[u]));
    }
}

void Ls74::evaluate(EvalContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        const FlipFlopPins& p = kUnits[u];
        LogicValue& state = state_[u];
        const LogicValue pre = ctx.read(p.pre);
        const LogicValue clr = ctx.read(p.clr);
        if (pre == X || clr == X) {
            state = X;
        } else if (pre == Zero && clr == Zero) {
            state = One;
            ctx.drive(p.q, One, delays::kLs74.rise);
            ctx.drive(p.qn, One, delays::kLs74.rise);
            continue;
        } else if (pre == Zero) {
            state = One;
        } else if (clr == Zero) {
            state = Zero;
        } else if (ctx.rose(p.clk)) {
            state = ctx.readBefore(p.d);
        }
        ctx.drive(p.q, state, DelayFor(delays::kLs74, state));
        ctx.drive(p.qn, Not(state), DelayFor(delays::kLs74, Not(state)));
    }
}

}  // namespace bw
