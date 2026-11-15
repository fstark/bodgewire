#include "parts/mc3446.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct Channel {
    PinIndex receiver, bus, driver, enable;
};

constexpr std::array<Channel, 4> kChannels{{
    {0, 1, 2, Mc3446::kEnAbc},
    {6, 5, 4, Mc3446::kEnAbc},
    {8, 9, 10, Mc3446::kEnAbc},
    {14, 13, 12, Mc3446::kEnD},
}};

std::vector<PinSpec> Mc3446Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto bus = [](const char* name) { return PinSpec{name, PinKind::PullUp, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {out("RA"), bus("BUSA"),  in("DA"),  in("ENABC_N"), in("DB"), bus("BUSB"),
            out("RB"), power("GND"), out("RC"), bus("BUSC"),   in("DC"), in("END_N"),
            in("DD"),  bus("BUSD"),  out("RD"), power("VCC")};
}

bool Changed(const EvalContext& ctx, PinIndex pin) { return ctx.rose(pin) || ctx.fell(pin); }

// The bus pin's drive: 0 while the driver and its enable are both low, 1 (released to the
// termination) while either is high, X otherwise.
LogicValue BusDrive(LogicValue driver, LogicValue enable) {
    if (driver == One || enable == One) {
        return One;
    }
    return driver == Zero && enable == Zero ? Zero : X;
}

}  // namespace

Mc3446::Mc3446(std::string reference) : Component(std::move(reference), Mc3446Pins()) {}

void Mc3446::evaluate(EvalContext& ctx) {
    for (const Channel& c : kChannels) {
        const LogicValue drive = BusDrive(ctx.read(c.driver), ctx.read(c.enable));
        const Delays delays =
            Changed(ctx, c.enable) ? delays::kMc3446Enable : delays::kMc3446Driver;
        ctx.drive(c.bus, drive, DelayFor(delays, drive));
        const LogicValue bus = ctx.read(c.bus);
        ctx.drive(c.receiver, bus, DelayFor(delays::kMc3446Receiver, bus));
    }
}

}  // namespace bw
