#include "parts/ls138.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 8> kOutputs{14, 13, 12, 11, 10, 9, 8, 6};  // Y0_N..Y7_N

std::vector<PinSpec> Ls138Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("A"),     in("B"),      in("C"),     in("G2A_N"), in("G2B_N"), in("G1"),
            out("Y7_N"), power("GND"), out("Y6_N"), out("Y5_N"), out("Y4_N"), out("Y3_N"),
            out("Y2_N"), out("Y1_N"),  out("Y0_N"), power("VCC")};
}

bool Changed(const EvalContext& ctx, PinIndex pin) { return ctx.rose(pin) || ctx.fell(pin); }

}  // namespace

Ls138::Ls138(std::string reference) : Component(std::move(reference), Ls138Pins()) {}

void Ls138::evaluate(EvalContext& ctx) {
    const LogicValue g1 = ctx.read(kG1);
    const LogicValue g2a = ctx.read(kG2aN);
    const LogicValue g2b = ctx.read(kG2bN);
    // Enabled: One, disabled: Zero, X when an X input could go either way.
    LogicValue enabled = One;
    if (g1 == Zero || g2a == One || g2b == One) {
        enabled = Zero;
    } else if (g1 != One || g2a != Zero || g2b != Zero) {
        enabled = X;
    }
    // The select values the inputs allow: a known bit fixes it, an X bit allows both.
    std::array<LogicValue, 3> bits{ctx.read(kA), ctx.read(kB), ctx.read(kC)};
    unsigned fixedMask = 0, fixedValue = 0;
    for (unsigned bit = 0; bit < 3; ++bit) {
        if (bits[bit] == Zero || bits[bit] == One) {
            fixedMask |= 1u << bit;
            fixedValue |= (bits[bit] == One ? 1u : 0u) << bit;
        }
    }
    const bool exact = fixedMask == 7;
    const Delays delays = Changed(ctx, kG1)                            ? delays::kLs138EnableG1
                          : Changed(ctx, kG2aN) || Changed(ctx, kG2bN) ? delays::kLs138EnableG2
                                                                       : delays::kLs138Select;
    for (unsigned k = 0; k < kOutputs.size(); ++k) {
        LogicValue y = One;
        if (enabled != Zero && (k & fixedMask) == fixedValue) {
            y = enabled == One && exact ? Zero : X;
        }
        ctx.drive(kOutputs[k], y, DelayFor(delays, y));
    }
}

}  // namespace bw
