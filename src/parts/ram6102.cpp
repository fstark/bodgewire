#include "parts/ram6102.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> Ram6102Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Mos}; };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    // DIN is read only while a write is in progress or as it ends, at a wake by another Pin.
    return {in("A6"),
            in("A5"),
            in("RW"),
            in("A1"),
            in("A2"),
            in("A3"),
            in("A4"),
            in("A0"),
            power("GND"),
            power("VCC"),
            PinSpec{"DIN", PinKind::Input, Family::Mos, false, true},
            PinSpec{"DOUT", PinKind::Output, Family::Mos},
            in("CE_N"),
            in("A9"),
            in("A8"),
            in("A7")};
}

}  // namespace

Ram6102::Ram6102(std::string reference) : Component(std::move(reference), Ram6102Pins()) {}

void Ram6102::powerOn(PowerOnContext& ctx) {
    cells_.powerOn(ctx);
    ctx.setOutput(kDout, Z);
}

std::size_t Ram6102::wordCount(std::size_t memory) const {
    Check(memory == 0, "6102: one memory");
    return cells_.size();
}

std::optional<unsigned> Ram6102::dataBit(PinIndex pin) const {
    if (pin == kDin || pin == kDout) {
        return 0u;
    }
    return std::nullopt;
}

void Ram6102::stick(std::size_t memory, std::size_t word, unsigned bit, bool one) {
    Check(memory == 0, "6102: one memory");
    cells_.stick(word, bit, one);
}

std::uint8_t Ram6102::read(std::size_t memory, std::size_t word) const {
    Check(memory == 0 && word < cells_.size(), "6102: no such word");
    return cells_[word];
}

unsigned Ram6102::wordBits(std::size_t memory) const {
    Check(memory == 0, "6102: one memory");
    return 1;
}

void Ram6102::evaluate(EvalContext& ctx) {
    // A write in progress just before now: keep the bit it was writing (zero hold time).
    if (!ctx.readBitBefore(kCeN) && !ctx.readBitBefore(kRw)) {
        std::size_t address = 0;
        for (std::size_t i = 0; i < kAddress.size(); ++i) {
            address |= static_cast<std::size_t>(ctx.readBitBefore(kAddress[i])) << i;
        }
        cells_.store(address, ctx.readBitBefore(kDin) ? 1u : 0u);
    }

    const bool selected = !ctx.readBit(kCeN);
    const bool writing = selected && !ctx.readBit(kRw);
    std::size_t address = 0;
    for (std::size_t i = 0; i < kAddress.size(); ++i) {
        address |= static_cast<std::size_t>(ctx.readBit(kAddress[i])) << i;
    }
    if (writing) {
        cells_.store(address, ctx.readBit(kDin) ? 1u : 0u);
    }
    if (!selected || writing) {
        ctx.drive(kDout, Z, delays::k6102Off);
        return;
    }

    Time delay = 0;
    if (std::ranges::any_of(kAddress,
                            [&](PinIndex pin) { return ctx.read(pin) != ctx.readBefore(pin); })) {
        delay = delays::k6102Access;
    }
    if (ctx.read(kCeN) != ctx.readBefore(kCeN) || ctx.read(kRw) != ctx.readBefore(kRw)) {
        delay = std::max(delay, delays::k6102Select);
    }
    if (delay == 0) {
        delay = delays::k6102Access;  // power-on, or only DIN moved
    }
    ctx.drive(kDout, cells_[address] != 0 ? One : Zero, delay);
}

}  // namespace bw
