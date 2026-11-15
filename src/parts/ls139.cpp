#include "parts/ls139.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct Half {
    PinIndex g, a, b;
    std::array<PinIndex, 4> outputs;  // Y0_N..Y3_N
};

constexpr std::array<Half, 2> kHalves{{
    {Ls139::k1G, Ls139::k1A, Ls139::k1B, {3, 4, 5, 6}},
    {Ls139::k2G, Ls139::k2A, Ls139::k2B, {11, 10, 9, 8}},
}};

std::vector<PinSpec> Ls139Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("1G_N"),   in("1A"),     in("1B"),     out("1Y0_N"), out("1Y1_N"), out("1Y2_N"),
            out("1Y3_N"), power("GND"), out("2Y3_N"), out("2Y2_N"), out("2Y1_N"), out("2Y0_N"),
            in("2B"),     in("2A"),     in("2G_N"),   power("VCC")};
}

bool Changed(const EvalContext& ctx, PinIndex pin) { return ctx.rose(pin) || ctx.fell(pin); }

void EvaluateHalf(EvalContext& ctx, const Half& half) {
    const LogicValue g = ctx.read(half.g);
    // Enabled: One, disabled: Zero, X when an X enable could go either way.
    const LogicValue enabled = g == Zero ? One : g == One ? Zero : X;
    const std::array<LogicValue, 2> bits{ctx.read(half.a), ctx.read(half.b)};
    unsigned fixedMask = 0, fixedValue = 0;
    for (unsigned bit = 0; bit < 2; ++bit) {
        if (bits[bit] == Zero || bits[bit] == One) {
            fixedMask |= 1u << bit;
            fixedValue |= (bits[bit] == One ? 1u : 0u) << bit;
        }
    }
    const bool exact = fixedMask == 3;
    const Delays delays = Changed(ctx, half.g) ? delays::kLs139Enable : delays::kLs139Select;
    for (unsigned k = 0; k < half.outputs.size(); ++k) {
        LogicValue y = One;
        if (enabled != Zero && (k & fixedMask) == fixedValue) {
            y = enabled == One && exact ? Zero : X;
        }
        ctx.drive(half.outputs[k], y, DelayFor(delays, y));
    }
}

}  // namespace

Ls139::Ls139(std::string reference) : Component(std::move(reference), Ls139Pins()) {}

void Ls139::evaluate(EvalContext& ctx) {
    for (const Half& half : kHalves) {
        EvaluateHalf(ctx, half);
    }
}

}  // namespace bw
