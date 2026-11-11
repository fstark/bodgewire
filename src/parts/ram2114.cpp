#include "parts/ram2114.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "parts/delays.hpp"
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

Ram2114::Ram2114(std::string reference) : Component(std::move(reference), Ram2114Pins()) {}

void Ram2114::powerOn(PowerOnContext& ctx) {
    for (std::uint8_t& word : words_) {
        word = 0;
        for (int bit = 0; bit < 4; ++bit) {
            word = static_cast<std::uint8_t>(word | (ctx.randomBit() ? 1u << bit : 0u));
        }
    }
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
        words_[address] = static_cast<std::uint8_t>(data);
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
        words_[address] = static_cast<std::uint8_t>(data);
    }
    if (!selected || writing) {
        for (const PinIndex pin : kData) {
            ctx.drive(pin, Z, delays::k2114Off);
        }
        return;
    }

    Time delay = 0;
    if (std::ranges::any_of(kAddress,
                            [&](PinIndex pin) { return ctx.read(pin) != ctx.readBefore(pin); })) {
        delay = delays::k2114Address;
    }
    if (ctx.read(kCsN) != ctx.readBefore(kCsN) || ctx.read(kWeN) != ctx.readBefore(kWeN)) {
        delay = std::max(delay, delays::k2114Select);
    }
    if (delay == 0) {
        delay = delays::k2114Address;  // power-on, or only the data pins moved
    }
    for (std::size_t i = 0; i < kData.size(); ++i) {
        ctx.drive(kData[i], (words_[address] >> i) & 1 ? One : Zero, delay);
    }
}

}  // namespace bw
