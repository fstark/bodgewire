#include "parts/ls154.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 16> kOutputs{0, 1, 2,  3,  4,  5,  6,  7,
                                            8, 9, 10, 12, 13, 14, 15, 16};  // Y0_N..Y15_N

std::vector<PinSpec> Ls154Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {out("Y0_N"),  out("Y1_N"),  out("Y2_N"),  out("Y3_N"),  out("Y4_N"),  out("Y5_N"),
            out("Y6_N"),  out("Y7_N"),  out("Y8_N"),  out("Y9_N"),  out("Y10_N"), power("GND"),
            out("Y11_N"), out("Y12_N"), out("Y13_N"), out("Y14_N"), out("Y15_N"), in("G1_N"),
            in("G2_N"),   in("D"),      in("C"),      in("B"),      in("A"),      power("VCC")};
}

bool Changed(const EvalContext& ctx, PinIndex pin) { return ctx.rose(pin) || ctx.fell(pin); }

}  // namespace

Ls154::Ls154(std::string reference) : Component(std::move(reference), Ls154Pins()) {}

void Ls154::evaluate(EvalContext& ctx) {
    const LogicValue g1 = ctx.read(kG1N);
    const LogicValue g2 = ctx.read(kG2N);
    // Enabled: One, disabled: Zero, X when an X input could go either way.
    LogicValue enabled = One;
    if (g1 == One || g2 == One) {
        enabled = Zero;
    } else if (g1 != Zero || g2 != Zero) {
        enabled = X;
    }
    // The select values the inputs allow: a known bit fixes it, an X bit allows both.
    const std::array<LogicValue, 4> bits{ctx.read(kA), ctx.read(kB), ctx.read(kC), ctx.read(kD)};
    unsigned fixedMask = 0, fixedValue = 0;
    for (unsigned bit = 0; bit < 4; ++bit) {
        if (bits[bit] == Zero || bits[bit] == One) {
            fixedMask |= 1u << bit;
            fixedValue |= (bits[bit] == One ? 1u : 0u) << bit;
        }
    }
    const bool exact = fixedMask == 0xF;
    const Delays delays =
        Changed(ctx, kG1N) || Changed(ctx, kG2N) ? delays::kLs154Enable : delays::kLs154Select;
    for (unsigned k = 0; k < kOutputs.size(); ++k) {
        LogicValue y = One;
        if (enabled != Zero && (k & fixedMask) == fixedValue) {
            y = enabled == One && exact ? Zero : X;
        }
        ctx.drive(kOutputs[k], y, DelayFor(delays, y));
    }
}

}  // namespace bw
