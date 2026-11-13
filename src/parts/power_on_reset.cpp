#include "parts/power_on_reset.hpp"

#include <utility>
#include <vector>

#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> PowerOnResetPins() {
    const auto nc = [](const char* name) { return PinSpec{name, PinKind::NoConnect, Family::Ttl}; };
    return {PinSpec{"RST_N", PinKind::OpenCollector, Family::Ttl},
            nc("NC2"),
            nc("NC3"),
            PinSpec{"GND", PinKind::Power, Family::Ttl},
            nc("NC5"),
            nc("NC6"),
            nc("NC7"),
            PinSpec{"VCC", PinKind::Power, Family::Ttl}};
}

}  // namespace

PowerOnReset::PowerOnReset(std::string reference, Time hold)
    : Component(std::move(reference), PowerOnResetPins()), hold_(hold) {
    Check(hold >= 1, "power-on reset: hold must be >= 1 ps");
}

void PowerOnReset::powerOn(PowerOnContext& ctx) { ctx.setOutput(kRstN, Zero); }

// Called once, at power-on (it has no inputs): the release, `hold` later.
void PowerOnReset::evaluate(EvalContext& ctx) { ctx.drive(kRstN, One, hold_); }

}  // namespace bw
