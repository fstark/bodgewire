#include "parts/ls157.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

struct SelectorPins {
    PinIndex a, b, y;
};

constexpr std::array<SelectorPins, 4> kUnits{{{1, 2, 3}, {4, 5, 6}, {10, 9, 8}, {13, 12, 11}}};

std::vector<PinSpec> Ls157Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("S"),   in("1A"), in("1B"), out("1Y"), in("2A"), in("2B"), out("2Y"), power("GND"),
            out("3Y"), in("3B"), in("3A"), out("4Y"), in("4B"), in("4A"), in("G_N"), power("VCC")};
}

// Y for known S and G_N.
LogicValue Select(bool select, bool strobeN, LogicValue a, LogicValue b) {
    if (strobeN) {
        return Zero;
    }
    return select ? b : a;
}

// Y for every value an X on S or G_N could have: the value if all agree, X otherwise.
LogicValue Output(LogicValue select, LogicValue strobeN, LogicValue a, LogicValue b) {
    bool any = false;
    LogicValue result = X;
    for (const bool s : {false, true}) {
        if ((select == Zero && s) || (select == One && !s)) {
            continue;
        }
        for (const bool g : {false, true}) {
            if ((strobeN == Zero && g) || (strobeN == One && !g)) {
                continue;
            }
            const LogicValue y = Select(s, g, a, b);
            if (!any) {
                result = y;
                any = true;
            } else if (y != result) {
                return X;
            }
        }
    }
    return result;
}

}  // namespace

Ls157::Ls157(std::string reference) : Component(std::move(reference), Ls157Pins()) {}

void Ls157::evaluate(EvalContext& ctx) {
    const LogicValue select = ctx.read(kS);
    const LogicValue strobeN = ctx.read(kGN);
    const Delays delays = ctx.rose(kS) || ctx.fell(kS)     ? delays::kLs157Select
                          : ctx.rose(kGN) || ctx.fell(kGN) ? delays::kLs157Strobe
                                                           : delays::kLs157Data;
    for (const SelectorPins& unit : kUnits) {
        const LogicValue y = Output(select, strobeN, ctx.read(unit.a), ctx.read(unit.b));
        ctx.drive(unit.y, y, DelayFor(delays, y));
    }
}

}  // namespace bw
