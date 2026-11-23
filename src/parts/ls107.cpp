#include "parts/ls107.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct FlipFlopPins {
    PinIndex j, k, clk, clr, q, qn;
};

constexpr std::array<FlipFlopPins, 2> kUnits{{{0, 3, 11, 12, 2, 1}, {7, 10, 8, 9, 4, 5}}};

std::vector<PinSpec> Ls107Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    // J and K are read only before a falling clock edge: sampled. The clocks act only as they
    // fall (no X-clock path): falling-edge-only.
    const auto jk = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Ttl, false, true};
    };
    const auto clk = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Ttl, false, false, true};
    };
    return {jk("1J"), out("1Q_N"), out("1Q"),    jk("1K"), out("2Q"),   out("2Q_N"),  power("GND"),
            jk("2J"), clk("2CLK"), in("2CLR_N"), jk("2K"), clk("1CLK"), in("1CLR_N"), power("VCC")};
}

bool Known(LogicValue value) { return value == Zero || value == One; }

// JK table for known values: 0 0 hold, 1 0 set, 0 1 clear, 1 1 toggle.
bool NextKnown(bool j, bool k, bool q) { return (j && !q) || (!k && q); }

// The outcome over every 0/1 value of the unknown inputs: one value if they agree, else X.
LogicValue Next(LogicValue j, LogicValue k, LogicValue q) {
    bool seenZero = false, seenOne = false;
    for (int jb = 0; jb <= 1; ++jb) {
        if (Known(j) && (j == One) != (jb == 1)) {
            continue;
        }
        for (int kb = 0; kb <= 1; ++kb) {
            if (Known(k) && (k == One) != (kb == 1)) {
                continue;
            }
            for (int qb = 0; qb <= 1; ++qb) {
                if (Known(q) && (q == One) != (qb == 1)) {
                    continue;
                }
                (NextKnown(jb == 1, kb == 1, qb == 1) ? seenOne : seenZero) = true;
            }
        }
    }
    return seenZero && seenOne ? X : (seenOne ? One : Zero);
}

}  // namespace

Ls107::Ls107(std::string reference) : Component(std::move(reference), Ls107Pins()) {}

void Ls107::powerOn(PowerOnContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        state_[u] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kUnits[u].q, state_[u]);
        ctx.setOutput(kUnits[u].qn, Not(state_[u]));
    }
}

void Ls107::evaluate(EvalContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        const FlipFlopPins& p = kUnits[u];
        LogicValue& state = state_[u];
        const LogicValue clr = ctx.read(p.clr);
        if (clr == Zero) {
            state = Zero;
        } else if (clr == X) {
            state = X;
        } else if (ctx.fell(p.clk)) {
            state = Next(ctx.readBefore(p.j), ctx.readBefore(p.k), state);
        }
        ctx.drive(p.q, state, DelayFor(delays::kLs107, state));
        ctx.drive(p.qn, Not(state), DelayFor(delays::kLs107, Not(state)));
    }
}

}  // namespace bw
