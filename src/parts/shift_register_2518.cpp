#include "parts/shift_register_2518.hpp"

#include <utility>
#include <vector>

#include "parts/delays.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> ShiftRegister2518Pins() {
    const auto in = [](const char* name) { return PinSpec{name, PinKind::Input, Family::Mos}; };
    const auto out = [](const char* name) { return PinSpec{name, PinKind::Output, Family::Mos}; };
    return {in("IN4"),
            in("IN5"),
            in("IN6"),
            in("REC"),
            {"VGG", PinKind::NoConnect, Family::Mos},
            {"CLK", PinKind::Input, Family::Mos, true},
            out("OUT6"),
            out("OUT5"),
            out("OUT4"),
            out("OUT3"),
            out("OUT2"),
            out("OUT1"),
            in("IN1"),
            in("IN2"),
            in("IN3"),
            {"VCC", PinKind::Power, Family::Ttl}};
}

}  // namespace

ShiftRegister2518::ShiftRegister2518(std::string reference)
    : Component(std::move(reference), ShiftRegister2518Pins()) {}

void ShiftRegister2518::powerOn(PowerOnContext& ctx) {
    for (std::uint8_t& word : words_) {
        word = 0;
        for (int bit = 0; bit < 6; ++bit) {
            word = static_cast<std::uint8_t>(word | (ctx.randomBit() ? 1u << bit : 0u));
        }
    }
    head_ = 0;
    for (std::size_t i = 0; i < kOut.size(); ++i) {
        ctx.setOutput(kOut[i], (words_[0] >> i) & 1 ? One : Zero);
    }
}

void ShiftRegister2518::evaluate(EvalContext& ctx) {
    if (ctx.rose(kClk)) {
        if (!ctx.readBitBefore(kRec)) {
            unsigned word = 0;
            for (std::size_t i = 0; i < kIn.size(); ++i) {
                word |= static_cast<unsigned>(ctx.readBitBefore(kIn[i])) << i;
            }
            words_[head_] = static_cast<std::uint8_t>(word);
        }
        head_ = (head_ + 1) % kWords;
    }
    for (std::size_t i = 0; i < kOut.size(); ++i) {
        ctx.drive(kOut[i], (words_[head_] >> i) & 1 ? One : Zero, delays::k2518ClockToOutput);
    }
}

}  // namespace bw
