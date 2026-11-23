#include "parts/ttl177.hpp"

#include <array>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::array<PinIndex, 4> kOutputs{Ttl177::kQA, Ttl177::kQB, Ttl177::kQC, Ttl177::kQD};
constexpr std::array<PinIndex, 4> kInputs{Ttl177::kA, Ttl177::kB, Ttl177::kC, Ttl177::kD};
// The ÷8 is a ripple of three flip-flops: QB settles first, QD last.
constexpr std::array<Delays, 4> kClockDelays{delays::kTtl177Clk1, delays::kTtl177Qb,
                                             delays::kTtl177Qc, delays::kTtl177Qd};

std::vector<PinSpec> Ttl177Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Ttl}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Ttl}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("CT_LOAD_N"), out("QC"), in("C"), in("A"), out("QA"), in("CLK2"),  power("GND"),
            in("CLK1"),      out("QB"), in("B"), in("D"), out("QD"), in("CLR_N"), power("VCC")};
}

LogicValue Level(unsigned bits, unsigned unknown, unsigned bit) {
    if ((unknown >> bit) & 1u) {
        return X;
    }
    return (bits >> bit) & 1u ? One : Zero;
}

bool Changed(const EvalContext& ctx, PinIndex pin) { return ctx.read(pin) != ctx.readBefore(pin); }

}  // namespace

Ttl177::Ttl177(std::string reference) : Component(std::move(reference), Ttl177Pins()) {
    declarePinGroup("A-D", {"A", "B", "C", "D"});
    declarePinGroup("QA-QD", {"QA", "QB", "QC", "QD"});
}

void Ttl177::powerOn(PowerOnContext& ctx) {
    count_ = 0;
    unknown_ = 0;
    for (unsigned bit = 0; bit < 4; ++bit) {
        count_ |= static_cast<std::uint8_t>((ctx.randomBit() ? 1u : 0u) << bit);
        ctx.setOutput(kOutputs[bit], Level(count_, 0, bit));
    }
}

void Ttl177::evaluate(EvalContext& ctx) {
    const LogicValue clear = ctx.read(kClrN);
    const LogicValue load = ctx.read(kLoadN);
    std::array<Delays, 4> delays = kClockDelays;

    if (clear == Zero) {
        count_ = 0;
        unknown_ = 0;
        delays.fill(delays::kTtl177Clear);
    } else if (clear != One) {
        unknown_ |= count_;  // an X clear could clear any 1
        delays.fill(delays::kTtl177Clear);
    } else if (load != One) {
        // Load low: each Q follows its data input; X load: Q is X where they differ.
        const Delays loadDelays = Changed(ctx, kLoadN) ? delays::kTtl177Load : delays::kTtl177Data;
        delays.fill(loadDelays);
        for (unsigned bit = 0; bit < 4; ++bit) {
            const LogicValue d = ctx.read(kInputs[bit]);
            const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit);
            const bool dKnown = d == Zero || d == One;
            const bool held = (count_ & mask) != 0;
            const bool heldKnown = (unknown_ & mask) == 0;
            if (load == Zero) {
                count_ = static_cast<std::uint8_t>(d == One ? (count_ | mask) : (count_ & ~mask));
                unknown_ =
                    static_cast<std::uint8_t>(dKnown ? (unknown_ & ~mask) : (unknown_ | mask));
            } else if (!dKnown || !heldKnown || (d == One) != held) {
                unknown_ |= mask;
            }
        }
    } else {
        // Counting. An X clock edge is not an edge (fell() sees clean edges only); an X clock
        // makes the bits it drives X.
        if (ctx.fell(kClk1)) {
            count_ ^= 1u;
        } else if (ctx.read(kClk1) != Zero && ctx.read(kClk1) != One) {
            unknown_ |= 1u;
        }
        if (ctx.fell(kClk2)) {
            // B toggles; C as B falls; D as C falls.
            unsigned b = (count_ >> 1) & 7u;
            b = (b + 1) & 7u;
            count_ = static_cast<std::uint8_t>((count_ & 1u) | (b << 1));
            // An unknown stage makes the stages after it unknown.
            if (unknown_ & 0b0010) {
                unknown_ |= 0b1110;
            } else if (unknown_ & 0b0100) {
                unknown_ |= 0b1100;
            }
        } else if (ctx.read(kClk2) != Zero && ctx.read(kClk2) != One) {
            unknown_ |= 0b1110;
        }
    }
    for (unsigned bit = 0; bit < 4; ++bit) {
        const LogicValue v = Level(count_, unknown_, bit);
        ctx.drive(kOutputs[bit], v, DelayFor(delays[bit], v));
    }
}

}  // namespace bw
