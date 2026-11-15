#include "parts/ttl145.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 10> kOutputs{0, 1, 2, 3, 4, 5, 6, 8, 9, 10};  // Y0_N..Y9_N

std::vector<PinSpec> Ttl145Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) {
        return PinSpec{name, PinKind::OpenCollector, Family::Ttl};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {out("Y0_N"), out("Y1_N"),  out("Y2_N"), out("Y3_N"), out("Y4_N"), out("Y5_N"),
            out("Y6_N"), power("GND"), out("Y7_N"), out("Y8_N"), out("Y9_N"), in("D"),
            in("C"),     in("B"),      in("A"),     power("VCC")};
}

}  // namespace

Ttl145::Ttl145(std::string reference, const Delays& delays)
    : Component(std::move(reference), Ttl145Pins()), delays_(delays) {}

void Ttl145::evaluate(EvalContext& ctx) {
    // The codes the inputs allow: a known bit fixes it, an X bit allows both (as the 74LS138).
    const std::array<LogicValue, 4> bits{ctx.read(kA), ctx.read(kB), ctx.read(kC), ctx.read(kD)};
    unsigned fixedMask = 0, fixedValue = 0;
    for (unsigned bit = 0; bit < 4; ++bit) {
        if (bits[bit] == Zero || bits[bit] == One) {
            fixedMask |= 1u << bit;
            fixedValue |= (bits[bit] == One ? 1u : 0u) << bit;
        }
    }
    const bool exact = fixedMask == 0xF;
    for (unsigned k = 0; k < kOutputs.size(); ++k) {
        LogicValue y = One;  // released
        if ((k & fixedMask) == fixedValue) {
            y = exact ? Zero : X;
        }
        ctx.drive(kOutputs[k], y, DelayFor(delays_, y));
    }
}

}  // namespace bw
