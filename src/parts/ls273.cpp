#include "parts/ls273.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct FlipFlopPins {
    PinIndex d, q;
};

constexpr std::array<FlipFlopPins, 8> kUnits{
    {{2, 1}, {3, 4}, {6, 5}, {7, 8}, {12, 11}, {13, 14}, {16, 15}, {17, 18}}};

std::vector<PinSpec> Ls273Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto d = [](const char* name) {  // read at the clock edge only: sampled
        return PinSpec{name, PinKind::Input, Family::Ttl, false, true};
    };
    return {in("CLR_N"),
            out("1Q"),
            d("1D"),
            d("2D"),
            out("2Q"),
            out("3Q"),
            d("3D"),
            d("4D"),
            out("4Q"),
            power("GND"),
            PinSpec{"CLK", PinKind::Input, Family::Ttl, true},
            out("5Q"),
            d("5D"),
            d("6D"),
            out("6Q"),
            out("7Q"),
            d("7D"),
            d("8D"),
            out("8Q"),
            power("VCC")};
}

}  // namespace

Ls273::Ls273(std::string reference) : Component(std::move(reference), Ls273Pins()) {}

void Ls273::powerOn(PowerOnContext& ctx) {
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        state_[u] = ctx.randomBit() ? One : Zero;
        ctx.setOutput(kUnits[u].q, state_[u]);
    }
}

void Ls273::evaluate(EvalContext& ctx) {
    const LogicValue clr = ctx.read(kClrN);
    const bool clocked = ctx.rose(kClk);
    const Delays delays = clr == Zero ? delays::kLs273Clear : delays::kLs273ClockToQ;
    for (std::size_t u = 0; u < kUnits.size(); ++u) {
        LogicValue& state = state_[u];
        if (clr == Zero) {
            state = Zero;
        } else if (clr == X) {
            state = X;
        } else if (clocked) {
            state = ctx.readBefore(kUnits[u].d);
        }
        ctx.drive(kUnits[u].q, state, DelayFor(delays, state));
    }
}

}  // namespace bw
