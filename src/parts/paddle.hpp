#pragma once

#include <atomic>
#include <expected>
#include <string>
#include <string_view>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr Time kPaddleDelay = 100'000;  // ps: TRIG_N fall to OUT, a 555's typical

// "30us" → ps, rounded to nearest; error text if malformed or < 1 ps. Netlist ParseDuration
// wraps it (as ParsePeriodFromFrequency wraps PeriodFromFrequency).
std::expected<Time, std::string> DurationFromText(std::string_view text);

// Paddle controller behind a KK-4 Connector: a pot and a 555 one-shot, logic only. Pins
// 1 TRIG_N, 2 OUT, 3 VCC, 4 GND (pin index = KiCad pad − 1). OUT is low while idle; a fall on
// TRIG_N starts a high pulse of shortest + knob × (longest − shortest), or longest − knob ×
// (longest − shortest) when `reversed` (the pot wired the other way round); falls during it are
// ignored.
class Paddle final : public Component {
public:
    // Check(1 <= shortest < longest).
    Paddle(std::string reference, Time shortest, Time longest, bool reversed = false);
    static constexpr PinIndex kTrigN = 0, kOut = 1;

    // Any thread; clamped to 0..1. Read when a pulse starts, so a turn needs no wake.
    void setKnob(double turn);
    double knob() const { return knob_.load(std::memory_order_relaxed); }
    Time pulseLength() const;  // for the knob now

    void powerOn(PowerOnContext& ctx) override;  // idle, OUT low; the knob untouched
    void evaluate(EvalContext& ctx) override;

private:
    Time shortest_, longest_;
    bool reversed_;
    std::atomic<double> knob_{0.0};
    bool busy_ = false;
    Time endsAt_ = 0;  // ps; while busy_: when the pulse ends on the TRIG_N side
};

}  // namespace bw
