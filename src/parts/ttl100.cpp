#include "parts/ttl100.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> Ttl100Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto nc = [](const char* name) { return PinSpec{name, PinKind::NoConnect, Family::Ttl}; };
    return {nc("NC1"),    in("1D1"),  in("1D2"),  out("1Q2"), out("1Q1"), nc("NC6"),
            power("GND"), out("2Q1"), out("2Q2"), in("2D2"),  in("2D1"),  in("2G"),
            nc("NC13"),   nc("NC14"), in("2D3"),  in("2D4"),  out("2Q4"), out("2Q3"),
            out("1Q3"),   out("1Q4"), in("1D4"),  in("1D3"),  in("1G"),   power("VCC")};
}

bool Known(LogicValue v) { return v == Zero || v == One; }

}  // namespace

Ttl100::Ttl100(std::string reference) : Component(std::move(reference), Ttl100Pins()) {}

void Ttl100::powerOn(PowerOnContext& ctx) {
    for (std::size_t half = 0; half < 2; ++half) {
        for (std::size_t bit = 0; bit < 4; ++bit) {
            q_[half][bit] = ctx.randomBit() ? One : Zero;
            ctx.setOutput(kQ[half][bit], q_[half][bit]);
        }
    }
}

void Ttl100::evaluate(EvalContext& ctx) {
    for (std::size_t half = 0; half < 2; ++half) {
        const PinIndex g = kEnable[half];
        const LogicValue enable = ctx.read(g);
        const bool enableChanged = enable != ctx.readBefore(g);
        const Delays delays = enableChanged ? delays::kTtl100Enable : delays::kTtl100Data;
        for (std::size_t bit = 0; bit < 4; ++bit) {
            LogicValue& q = q_[half][bit];
            if (enable == One) {
                q = ctx.read(kD[half][bit]);
                if (!Known(q)) {
                    q = X;
                }
            } else if (enable == Zero) {
                if (ctx.fell(g)) {
                    // Held: D from just before the fall.
                    q = ctx.readBefore(kD[half][bit]);
                    if (!Known(q)) {
                        q = X;
                    }
                }
            } else {
                const LogicValue d = ctx.read(kD[half][bit]);
                if (d != q) {
                    q = X;
                }
            }
            ctx.drive(kQ[half][bit], q, DelayFor(delays, q));
        }
    }
}

}  // namespace bw
