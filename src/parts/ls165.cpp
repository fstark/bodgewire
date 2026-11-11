#include "parts/ls165.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 8> kParallel{Ls165::kA, Ls165::kB, Ls165::kC, Ls165::kD,
                                            Ls165::kE, Ls165::kF, Ls165::kG, Ls165::kH};

std::vector<PinSpec> Ls165Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    // A fall of CLK never shifts (the OR with CLK_INH either stays high or falls).
    const PinSpec clk{"CLK", PinKind::Input, Family::Ttl, true};
    return {in("SH_LD_N"), clk,          in("E"),       in("F"),     in("G"), in("H"),
            out("QH_N"),   power("GND"), out("QH"),     in("SER"),   in("A"), in("B"),
            in("C"),       in("D"),      in("CLK_INH"), power("VCC")};
}

LogicValue Or(LogicValue a, LogicValue b) {
    const std::array<LogicValue, 2> inputs{a, b};
    return GateFunction(GateKind::Or, inputs);
}

}  // namespace

Ls165::Ls165(std::string reference) : Component(std::move(reference), Ls165Pins()) {}

void Ls165::powerOn(PowerOnContext& ctx) {
    for (LogicValue& stage : stages_) {
        stage = ctx.randomBit() ? One : Zero;
    }
    ctx.setOutput(kQH, stages_[7]);
    ctx.setOutput(kQHN, Not(stages_[7]));
}

void Ls165::evaluate(EvalContext& ctx) {
    const LogicValue load = ctx.read(kShLdN);
    Delays qDelays = delays::kLs165Clock;
    if (load == Zero) {
        for (std::size_t i = 0; i < stages_.size(); ++i) {
            stages_[i] = ctx.read(kParallel[i]);
        }
        qDelays = ctx.fell(kShLdN) ? delays::kLs165Load : delays::kLs165Data;
    } else if (load == X) {
        for (std::size_t i = 0; i < stages_.size(); ++i) {
            if (stages_[i] != ctx.read(kParallel[i])) {
                stages_[i] = X;
            }
        }
        qDelays = delays::kLs165Load;
    } else if (ctx.readBefore(kShLdN) == One) {
        const LogicValue before = Or(ctx.readBefore(kClk), ctx.readBefore(kClkInh));
        const LogicValue now = Or(ctx.read(kClk), ctx.read(kClkInh));
        const bool edge = before == Zero && now == One;
        const bool maybe = (before == Zero && now == X) || (before == X && now == One);
        if (edge || maybe) {
            std::array<LogicValue, 8> shifted{};
            shifted[0] = ctx.readBefore(kSer);
            std::copy(stages_.begin(), stages_.end() - 1, shifted.begin() + 1);
            for (std::size_t i = 0; i < stages_.size(); ++i) {
                stages_[i] = edge || stages_[i] == shifted[i] ? shifted[i] : X;
            }
        }
    }
    ctx.drive(kQH, stages_[7], DelayFor(qDelays, stages_[7]));
    ctx.drive(kQHN, Not(stages_[7]), DelayFor(qDelays, Not(stages_[7])));
}

}  // namespace bw
