#include "parts/ls48.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "parts/ls47.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

// Same pin indices as the 74LS47: ReadSegments reads the inputs by them.
static_assert(Ls48::kA == Ls47::kA && Ls48::kB == Ls47::kB && Ls48::kC == Ls47::kC &&
              Ls48::kD == Ls47::kD && Ls48::kLtN == Ls47::kLtN && Ls48::kRbiN == Ls47::kRbiN &&
              Ls48::kBiN == Ls47::kBiN);

constexpr std::array<PinIndex, 7> kSegments{Ls48::kSA, Ls48::kSB, Ls48::kSC, Ls48::kSD,
                                            Ls48::kSE, Ls48::kSF, Ls48::kSG};

std::vector<PinSpec> Ls48Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("B"),   in("C"),      in("LT_N"), in("BI_N"),  in("RBI_N"), in("D"),
            in("A"),   power("GND"), out("SE"),  out("SD"),   out("SC"),   out("SB"),
            out("SA"), out("SG"),    out("SF"),  power("VCC")};
}

}  // namespace

Ls48::Ls48(std::string reference) : Component(std::move(reference), Ls48Pins()) {
    declareBus("A-D", {"A", "B", "C", "D"});
}

void Ls48::evaluate(EvalContext& ctx) {
    const SegmentCertainty segments = ReadSegments(ctx);
    for (std::size_t k = 0; k < kSegments.size(); ++k) {
        const LogicValue value =
            (segments.lit >> k) & 1u ? One : ((segments.dark >> k) & 1u ? Zero : X);
        ctx.drive(kSegments[k], value, DelayFor(delays::kLs48, value));
    }
}

}  // namespace bw
