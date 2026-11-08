#include "parts/ls47.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

// Glyphs for 0..15: 6 without a, 9 without d, 10..14 the datasheet's fragments, 15 blank.
constexpr std::array<std::uint8_t, 16> kGlyphs{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07,
                                               0x7F, 0x67, 0x58, 0x4C, 0x62, 0x69, 0x78, 0x00};

// Inputs in the order Ls47Segments takes them: A, B, C, D, LT_N, RBI_N, BI_N.
constexpr std::array<PinIndex, 7> kInputs{Ls47::kA,   Ls47::kB,    Ls47::kC,  Ls47::kD,
                                          Ls47::kLtN, Ls47::kRbiN, Ls47::kBiN};
constexpr std::array<PinIndex, 7> kSegments{Ls47::kSA, Ls47::kSB, Ls47::kSC, Ls47::kSD,
                                            Ls47::kSE, Ls47::kSF, Ls47::kSG};

std::vector<PinSpec> Ls47Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) {
        return PinSpec{name, PinKind::OpenCollector, Family::Ttl};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("B"),     in("C"),      in("LT_N"),  in("BI_N"),  in("RBI_N"), in("D"),
            in("A"),     power("GND"), out("SE_N"), out("SD_N"), out("SC_N"), out("SB_N"),
            out("SA_N"), out("SG_N"),  out("SF_N"), power("VCC")};
}

std::uint8_t SegmentsOf(unsigned bits) {
    return Ls47Segments(bits & 15u, (bits >> 4) & 1u, (bits >> 5) & 1u, (bits >> 6) & 1u);
}

}  // namespace

std::uint8_t Ls47Segments(unsigned value, bool lampTestN, bool rippleBlankingInN,
                          bool blankingInN) {
    if (!blankingInN) {
        return 0;
    }
    if (!lampTestN) {
        return 0x7F;
    }
    if (!rippleBlankingInN && value == 0) {
        return 0;
    }
    return kGlyphs[value & 15u];
}

Ls47::Ls47(std::string reference) : Component(std::move(reference), Ls47Pins()) {}

void Ls47::evaluate(EvalContext& ctx) {
    unsigned known = 0;    // input bits read as 1
    unsigned unknown = 0;  // input bits at X
    for (std::size_t i = 0; i < kInputs.size(); ++i) {
        const LogicValue value = ctx.read(kInputs[i]);
        if (value == One) {
            known |= 1u << i;
        } else if (value == X) {
            unknown |= 1u << i;
        }
    }
    std::uint8_t on = 0x7F;   // lit for every value the X inputs may have
    std::uint8_t off = 0x7F;  // dark for every one
    for (unsigned subset = unknown;; subset = (subset - 1) & unknown) {
        const std::uint8_t lit = SegmentsOf(known | subset);
        on &= lit;
        off &= static_cast<std::uint8_t>(~lit);
        if (subset == 0) {
            break;
        }
    }
    for (std::size_t k = 0; k < kSegments.size(); ++k) {
        const LogicValue value = (on >> k) & 1u ? Zero : ((off >> k) & 1u ? One : X);
        ctx.drive(kSegments[k], value, DelayFor(delays::kLs47, value));
    }
}

}  // namespace bw
