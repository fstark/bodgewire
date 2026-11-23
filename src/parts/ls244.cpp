#include "parts/ls244.hpp"

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

constexpr std::array<BufferPins, 8> kBuffers{{{0, 1, 17},
                                              {0, 3, 15},
                                              {0, 5, 13},
                                              {0, 7, 11},
                                              {18, 10, 8},
                                              {18, 12, 6},
                                              {18, 14, 4},
                                              {18, 16, 2}}};

std::vector<PinSpec> Ls244Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("1OE_N"), in("1A1"),  out("2Y4"),   in("1A2"),  out("2Y3"),  in("1A3"),   out("2Y2"),
            in("1A4"),   out("2Y1"), power("GND"), in("2A1"),  out("1Y4"),  in("2A2"),   out("1Y3"),
            in("2A3"),   out("1Y2"), in("2A4"),    out("1Y1"), in("2OE_N"), power("VCC")};
}

}  // namespace

Ls244::Ls244(std::string reference) : Component(std::move(reference), Ls244Pins()) {}

void Ls244::powerOn(PowerOnContext& ctx) {
    for (std::size_t i = 0; i < kBuffers.size(); ++i) {
        ctx.setOutput(kBuffers[i].y, Z);
        driven_[i] = Z;
    }
    quiet_ = {};  // the Simulator made every Pin loud
}

void Ls244::evaluate(EvalContext& ctx) {
    for (std::size_t i = 0; i < kBuffers.size(); ++i) {
        const BufferPins& b = kBuffers[i];
        const LogicValue oe = ctx.read(b.oe);
        LogicValue value = X;
        if (oe == One) {
            value = Z;
        } else if (oe == Zero) {
            value = ctx.read(b.a) == One ? One : ctx.read(b.a) == Zero ? Zero : X;
        }
        Time delay = 0;
        if (value == Z) {
            delay = driven_[i] == One ? delays::kLs244Disable.rise : delays::kLs244Disable.fall;
        } else if (driven_[i] == Z) {
            delay = DelayFor(delays::kLs244Enable, value);
        } else {
            delay = DelayFor(delays::kLs244Data, value);
        }
        driven_[i] = value;
        ctx.drive(b.y, value, delay);
    }
    for (std::size_t half = 0; half < quiet_.size(); ++half) {
        const PinIndex oe = kBuffers[half * 4].oe;
        const bool off = ctx.read(oe) == One;
        if (off != quiet_[half]) {
            quiet_[half] = off;
            for (std::size_t i = half * 4; i < half * 4 + 4; ++i) {
                ctx.quiet(kBuffers[i].a, off);
            }
        }
    }
}

}  // namespace bw
