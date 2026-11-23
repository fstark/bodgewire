#include "parts/ls93.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/gate_function.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 3> kEightOutputs{Ls93::kQB, Ls93::kQC, Ls93::kQD};
// The ÷8 is a ripple of three flip-flops: QB settles first, QD last.
constexpr std::array<Delays, 3> kEightClockDelays{delays::kLs93CkbToQB, delays::kLs93CkbToQC,
                                                  delays::kLs93CkbToQD};

std::vector<PinSpec> Ls93Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    const auto nc = [](const char* name) { return PinSpec{name, PinKind::NoConnect, Family::Ttl}; };
    // The clocks act only as they fall (no X-clock path): falling-edge-only.
    const auto clk = [](const char* name) {
        return PinSpec{name, PinKind::Input, Family::Ttl, false, false, true};
    };
    return {clk("CKB"), in("R01"), in("R02"),    nc("NC4"), power("VCC"), nc("NC6"),  nc("NC7"),
            out("QC"),  out("QB"), power("GND"), out("QD"), out("QA"),    nc("NC13"), clk("CKA")};
}

LogicValue Level(bool bit, bool unknown) { return unknown ? X : (bit ? One : Zero); }

}  // namespace

Ls93::Ls93(std::string reference) : Component(std::move(reference), Ls93Pins()) {
    declarePinGroup("QA-QD", {"QA", "QB", "QC", "QD"});
}

void Ls93::powerOn(PowerOnContext& ctx) {
    unknownA_ = false;
    unknownEight_ = false;
    qa_ = ctx.randomBit();
    eight_ = 0;
    for (std::size_t bit = 0; bit < kEightOutputs.size(); ++bit) {
        if (ctx.randomBit()) {
            eight_ |= static_cast<std::uint8_t>(1u << bit);
        }
        ctx.setOutput(kEightOutputs[bit], Level((eight_ >> bit) & 1u, false));
    }
    ctx.setOutput(kQA, Level(qa_, false));
}

void Ls93::evaluate(EvalContext& ctx) {
    const std::array<LogicValue, 2> zero{ctx.read(kR01), ctx.read(kR02)};
    const LogicValue r0 = GateFunction(GateKind::And, zero);

    Delays aDelays = delays::kLs93CkaToQA;
    std::array<Delays, 3> eightDelays = kEightClockDelays;
    if (r0 == One) {
        qa_ = false;
        eight_ = 0;
        unknownA_ = unknownEight_ = false;
        aDelays = delays::kLs93Clear;
        eightDelays.fill(delays::kLs93Clear);
    } else if (r0 == X) {
        unknownA_ = unknownEight_ = true;
    } else {
        if (ctx.fell(kCka) && !unknownA_) {
            qa_ = !qa_;
        }
        if (ctx.fell(kCkb) && !unknownEight_) {
            eight_ = static_cast<std::uint8_t>((eight_ + 1) & 7u);
        }
    }

    const LogicValue a = Level(qa_, unknownA_);
    ctx.drive(kQA, a, DelayFor(aDelays, a));
    for (std::size_t bit = 0; bit < kEightOutputs.size(); ++bit) {
        const LogicValue value = Level((eight_ >> bit) & 1u, unknownEight_);
        ctx.drive(kEightOutputs[bit], value, DelayFor(eightDelays[bit], value));
    }
}

}  // namespace bw
