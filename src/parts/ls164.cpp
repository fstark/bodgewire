#include "parts/ls164.hpp"

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

constexpr std::array<PinIndex, 8> kOutputs{Ls164::kQA, Ls164::kQB, Ls164::kQC, Ls164::kQD,
                                           Ls164::kQE, Ls164::kQF, Ls164::kQG, Ls164::kQH};

std::vector<PinSpec> Ls164Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const PinSpec clk{"CLK", PinKind::Input, Family::Ttl, true};
    return {in("A"), in("B"),     out("QA"), out("QB"), out("QC"), out("QD"), power("GND"),
            clk,     in("CLR_N"), out("QE"), out("QF"), out("QG"), out("QH"), power("VCC")};
}

}  // namespace

Ls164::Ls164(std::string reference) : Component(std::move(reference), Ls164Pins()) {}

void Ls164::powerOn(PowerOnContext& ctx) {
    for (std::size_t i = 0; i < kOutputs.size(); ++i) {
        stages_[i] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kOutputs[i], stages_[i]);
    }
}

void Ls164::evaluate(EvalContext& ctx) {
    const LogicValue clr = ctx.read(kClrN);
    const bool edge = ctx.rose(kClk);
    Delays qDelays = delays::kLs164ClockToQ;
    if (clr == Zero) {
        stages_.fill(Zero);
        qDelays = delays::kLs164Clear;
    } else if (clr == X) {
        stages_.fill(X);
    } else if (edge) {
        const std::array<LogicValue, 2> serial{ctx.readBefore(kA), ctx.readBefore(kB)};
        std::shift_right(stages_.begin(), stages_.end(), 1);
        stages_[0] = GateFunction(GateKind::And, serial);
    }
    for (std::size_t i = 0; i < kOutputs.size(); ++i) {
        ctx.drive(kOutputs[i], stages_[i], DelayFor(qDelays, stages_[i]));
    }
}

}  // namespace bw
