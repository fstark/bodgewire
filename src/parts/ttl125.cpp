#include "parts/ttl125.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct BufferPins {
    PinIndex oe, a, y;
};

constexpr std::array<BufferPins, 4> kBuffers{{{0, 1, 2}, {3, 4, 5}, {9, 8, 7}, {12, 11, 10}}};

std::vector<PinSpec> Ttl125Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("1OE_N"), in("1A"), out("1Y"),   in("2OE_N"), in("2A"), out("2Y"),   power("GND"),
            out("3Y"),   in("3A"), in("3OE_N"), out("4Y"),   in("4A"), in("4OE_N"), power("VCC")};
}

}  // namespace

Ttl125::Ttl125(std::string reference) : Component(std::move(reference), Ttl125Pins()) {}

void Ttl125::powerOn(PowerOnContext& ctx) {
    for (std::size_t i = 0; i < kBuffers.size(); ++i) {
        ctx.setOutput(kBuffers[i].y, Z);
        driven_[i] = Z;
    }
}

void Ttl125::evaluate(EvalContext& ctx) {
    for (std::size_t i = 0; i < kBuffers.size(); ++i) {
        const BufferPins& b = kBuffers[i];
        const LogicValue oe = ctx.read(b.oe);
        LogicValue value = X;
        if (oe == One) {
            value = Z;
        } else if (oe == Zero) {
            const LogicValue a = ctx.read(b.a);
            value = a == One ? One : a == Zero ? Zero : X;
        }
        Time delay = 0;
        if (value == Z) {
            delay = driven_[i] == One ? delays::kTtl125Disable.rise : delays::kTtl125Disable.fall;
        } else if (driven_[i] == Z) {
            delay = DelayFor(delays::kTtl125Enable, value);
        } else {
            delay = DelayFor(delays::kTtl125Data, value);
        }
        driven_[i] = value;
        ctx.drive(b.y, value, delay);
    }
}

}  // namespace bw
