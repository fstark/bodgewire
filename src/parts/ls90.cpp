#include "parts/ls90.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

// ÷5 next state from the chip's JK flip-flops (B: J = !QD, K = 1; C toggles on B falling;
// D: J = QB·QC, K = 1): 0..4 count, 5 and 6 go to 2, 7 to 0.
constexpr std::array<std::uint8_t, 8> kNextFive{1, 2, 3, 4, 0, 2, 2, 0};
constexpr std::array<PinIndex, 3> kFiveOutputs{Ls90::kQB, Ls90::kQC, Ls90::kQD};
constexpr std::array<Delays, 3> kFiveClockDelays{delays::kLs90CkbToQB, delays::kLs90CkbToQC,
                                                 delays::kLs90CkbToQD};

std::vector<PinSpec> Ls90Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto nc = [](const char* name) { return PinSpec{name, PinKind::NoConnect, Family::Ttl}; };
    return {in("CKB"), in("R01"), in("R02"),    nc("NC4"), power("VCC"), in("R91"),  in("R92"),
            out("QC"), out("QB"), power("GND"), out("QD"), out("QA"),    nc("NC13"), in("CKA")};
}

LogicValue Level(bool bit, bool unknown) { return unknown ? X : (bit ? One : Zero); }

}  // namespace

Ls90::Ls90(std::string reference) : Component(std::move(reference), Ls90Pins()) {}

void Ls90::powerOn(PowerOnContext& ctx) {
    unknownA_ = false;
    unknownFive_ = false;
    qa_ = ctx.randomBit();
    five_ = 0;
    for (std::size_t bit = 0; bit < kFiveOutputs.size(); ++bit) {
        if (ctx.randomBit()) {
            five_ |= static_cast<std::uint8_t>(1u << bit);
        }
        ctx.setOutput(kFiveOutputs[bit], Level((five_ >> bit) & 1u, false));
    }
    ctx.setOutput(kQA, Level(qa_, false));
}

void Ls90::evaluate(EvalContext& ctx) {
    const std::array<LogicValue, 2> nine{ctx.read(kR91), ctx.read(kR92)};
    const std::array<LogicValue, 2> zero{ctx.read(kR01), ctx.read(kR02)};
    const LogicValue r9 = GateFunction(GateKind::And, nine);
    const LogicValue r0 = GateFunction(GateKind::And, zero);

    Delays aDelays = delays::kLs90CkaToQA;
    std::array<Delays, 3> fiveDelays = kFiveClockDelays;
    if (r9 == One) {
        qa_ = true;
        five_ = 4;
        unknownA_ = unknownFive_ = false;
        aDelays = delays::kLs90SetNine;
        fiveDelays.fill(delays::kLs90SetNine);
    } else if (r9 == X || r0 == X) {
        unknownA_ = unknownFive_ = true;
    } else if (r0 == One) {
        qa_ = false;
        five_ = 0;
        unknownA_ = unknownFive_ = false;
        aDelays = delays::kLs90Clear;
        fiveDelays.fill(delays::kLs90Clear);
    } else {
        if (ctx.fell(kCka) && !unknownA_) {
            qa_ = !qa_;
        }
        if (ctx.fell(kCkb) && !unknownFive_) {
            five_ = kNextFive[five_];
        }
    }

    const LogicValue a = Level(qa_, unknownA_);
    ctx.drive(kQA, a, DelayFor(aDelays, a));
    for (std::size_t bit = 0; bit < kFiveOutputs.size(); ++bit) {
        const LogicValue value = Level((five_ >> bit) & 1u, unknownFive_);
        ctx.drive(kFiveOutputs[bit], value, DelayFor(fiveDelays[bit], value));
    }
}

}  // namespace bw
