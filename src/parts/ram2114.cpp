#include "parts/ram2114.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> Ram2114Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Mos}; };
    const auto io = [](const char* name) {
        return PinSpec{name, PinKind::Bidirectional, Family::Mos};
    };
    const auto power = [](const char* name) { return PinSpec{name, PinKind::Power, Family::Ttl}; };
    return {in("A6"),  in("A5"),   in("A4"),     in("A3"),   in("A0"),  in("A1"),
            in("A2"),  in("CS_N"), power("GND"), in("WE_N"), io("IO4"), io("IO3"),
            io("IO2"), io("IO1"),  in("A9"),     in("A8"),   in("A7"),  power("VCC")};
}

}  // namespace

Ram2114::Ram2114(std::string reference, Time access)
    : Component(std::move(reference), Ram2114Pins()), access_(access) {}

void Ram2114::powerOn(PowerOnContext& ctx) {
    cells_.powerOn(ctx);
    dataReady_ = 0;
}

std::size_t Ram2114::wordCount(std::size_t memory) const {
    Check(memory == 0, "2114: one memory");
    return cells_.size();
}

std::optional<unsigned> Ram2114::dataBit(PinIndex pin) const {
    const auto it = std::ranges::find(kData, pin);
    if (it == kData.end()) {
        return std::nullopt;
    }
    return static_cast<unsigned>(it - kData.begin());
}

void Ram2114::stick(std::size_t memory, std::size_t word, unsigned bit, bool one) {
    Check(memory == 0, "2114: one memory");
    cells_.stick(word, bit, one);
}

void Ram2114::evaluate(EvalContext& ctx) {
    // A write in progress just before now: keep the word it was writing (zero hold time).
    if (!ctx.readBitBefore(kCsN) && !ctx.readBitBefore(kWeN)) {
        std::size_t address = 0;
        for (std::size_t i = 0; i < kAddress.size(); ++i) {
            address |= static_cast<std::size_t>(ctx.readBitBefore(kAddress[i])) << i;
        }
        unsigned data = 0;
        for (std::size_t i = 0; i < kData.size(); ++i) {
            data |= static_cast<unsigned>(ctx.readBitBefore(kData[i])) << i;
        }
        cells_.store(address, data);
    }

    const bool addressMoved = std::ranges::any_of(
        kAddress, [&](PinIndex pin) { return ctx.read(pin) != ctx.readBefore(pin); });
    if (addressMoved) {
        dataReady_ = ctx.now() + access_;
    }
    const bool selected = !ctx.readBit(kCsN);
    const bool writing = selected && !ctx.readBit(kWeN);
    std::size_t address = 0;
    for (std::size_t i = 0; i < kAddress.size(); ++i) {
        address |= static_cast<std::size_t>(ctx.readBit(kAddress[i])) << i;
    }
    if (writing) {
        unsigned data = 0;
        for (std::size_t i = 0; i < kData.size(); ++i) {
            data |= static_cast<unsigned>(ctx.readBit(kData[i])) << i;
        }
        cells_.store(address, data);
    }
    if (!selected || writing) {
        for (const PinIndex pin : kData) {
            ctx.drive(pin, Z, delays::k2114Off);
        }
        return;
    }

    // The data comes at the later of the address's time plus access (tAA) and the select's time
    // plus tCO: a chip selected after its address settled may still be waiting for the address.
    Time delay = 0;
    if (addressMoved) {
        delay = access_;
    }
    if (ctx.read(kCsN) != ctx.readBefore(kCsN) || ctx.read(kWeN) != ctx.readBefore(kWeN)) {
        delay = std::max({delay, delays::k2114Select, dataReady_ - ctx.now()});
    }
    if (delay == 0) {
        delay = access_;  // power-on, or only the data pins moved
    }
    for (std::size_t i = 0; i < kData.size(); ++i) {
        ctx.drive(kData[i], (cells_[address] >> i) & 1 ? One : Zero, delay);
    }
}

}  // namespace bw
