#include "parts/ls161.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 4> kData{Ls161::kA, Ls161::kB, Ls161::kC, Ls161::kD};
constexpr std::array<PinIndex, 4> kOutputs{Ls161::kQA, Ls161::kQB, Ls161::kQC, Ls161::kQD};

std::vector<PinSpec> Ls161Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("CLR_N"), in("CLK"),    in("A"),      in("B"),     in("C"),   in("D"),
            in("ENP"),   power("GND"), in("LOAD_N"), in("ENT"),   out("QD"), out("QC"),
            out("QB"),   out("QA"),    out("RCO"),   power("VCC")};
}

LogicValue Bit(std::uint8_t count, std::size_t bit) { return (count >> bit) & 1u ? One : Zero; }

}  // namespace

Ls161::Ls161(std::string reference) : Component(std::move(reference), Ls161Pins()) {}

void Ls161::powerOn(PowerOnContext& ctx) {
    count_ = 0;
    unknown_ = false;
    for (std::size_t bit = 0; bit < kOutputs.size(); ++bit) {
        if (ctx.randomBit()) {
            count_ |= static_cast<std::uint8_t>(1u << bit);
        }
        ctx.setOutput(kOutputs[bit], Bit(count_, bit));
    }
    ctx.setOutput(kRco, Zero);  // ENT not known yet
}

void Ls161::evaluate(EvalContext& ctx) {
    const LogicValue clr = ctx.read(kClrN);
    const bool edge = ctx.rose(kClk);
    Delays qDelays = delays::kLs161ClockToQ;
    if (clr == Zero) {
        count_ = 0;
        unknown_ = false;
        qDelays = delays::kLs161Clear;
    } else if (clr == X) {
        unknown_ = true;
    } else if (edge) {
        const LogicValue load = ctx.readBefore(kLoadN);
        const LogicValue enp = ctx.readBefore(kEnp);
        const LogicValue ent = ctx.readBefore(kEnt);
        if (load == Zero) {
            count_ = 0;
            unknown_ = false;
            for (std::size_t bit = 0; bit < kData.size(); ++bit) {
                const LogicValue value = ctx.readBefore(kData[bit]);
                unknown_ |= value == X;
                if (value == One) {
                    count_ |= static_cast<std::uint8_t>(1u << bit);
                }
            }
        } else if (load == X) {
            unknown_ = true;
        } else if (enp == One && ent == One) {
            count_ = (count_ + 1) & 15u;
        } else if ((enp == X || ent == X) && enp != Zero && ent != Zero) {
            unknown_ = true;
        }
    }

    for (std::size_t bit = 0; bit < kOutputs.size(); ++bit) {
        const LogicValue value = unknown_ ? X : Bit(count_, bit);
        ctx.drive(kOutputs[bit], value, DelayFor(qDelays, value));
    }
    const std::array<LogicValue, 2> rcoInputs{ctx.read(kEnt),
                                              unknown_ ? X : (count_ == 15 ? One : Zero)};
    const LogicValue rco = GateFunction(GateKind::And, rcoInputs);
    ctx.drive(kRco, rco, DelayFor(edge ? delays::kLs161ClockToRco : delays::kLs161EntToRco, rco));
}

}  // namespace bw
