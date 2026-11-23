#include "parts/ls175.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct FlipFlopPins {
    PinIndex d, q, qn;
};

constexpr std::array<FlipFlopPins, 4> kUnits{{{3, 1, 2}, {4, 6, 5}, {11, 9, 10}, {12, 14, 13}}};

std::vector<PinSpec> Ls175Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto d = [](const char* name) {  // read at the clock edge only: sampled
        return PinSpec{name, PinKind::Input, Family::Ttl, false, true};
    };
    return {in("CLR_N"), out("1Q"),    out("1Q_N"),
            d("1D"),     d("2D"),      out("2Q_N"),
            out("2Q"),   power("GND"), PinSpec{"CLK", PinKind::Input, Family::Ttl, true},
            out("3Q"),   out("3Q_N"),  d("3D"),
            d("4D"),     out("4Q_N"),  out("4Q"),
            power("VCC")};
}

}  // namespace

Ls175::Ls175(std::string reference) : Component(std::move(reference), Ls175Pins()) {}

void Ls175::powerOn(PowerOnContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        state_[u] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kUnits[u].q, state_[u]);
        ctx.setOutput(kUnits[u].qn, Not(state_[u]));
    }
}

void Ls175::evaluate(EvalContext& ctx) {
    const LogicValue clr = ctx.read(kClrN);
    const bool clocked = ctx.rose(kClk);
    const Delays delays = clr == Zero ? delays::kLs175Clear : delays::kLs175ClockToQ;
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        const FlipFlopPins& p = kUnits[u];
        LogicValue& state = state_[u];
        if (clr == Zero) {
            state = Zero;
        } else if (clr == X) {
            state = X;
        } else if (clocked) {
            state = ctx.readBefore(p.d);
        }
        ctx.drive(p.q, state, DelayFor(delays, state));
        ctx.drive(p.qn, Not(state), DelayFor(delays, Not(state)));
    }
}

}  // namespace bw
