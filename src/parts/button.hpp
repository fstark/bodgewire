#pragma once

#include <atomic>
#include <string>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr Time kButtonDelay = 1;  // ps: a press shows right after the wake

// Push button between a rail and a signal. Pin index = KiCad pad − 1. Pressed: the signal Pin
// drives the rail's value (Strong, TTL), so it beats a pull-up and fights a TTL output like a
// second one would. Released: both Pins drive Z.
class Button final : public Component {
public:
    explicit Button(std::string reference);

    void attach(PinIndex signal, LogicValue rail);  // netlist builder, before finalize()
    PinIndex signal() const { return signal_; }

    // Any thread. Takes effect at the next evaluation: follow with Simulator or Runner wake().
    void setPressed(bool pressed) { pressed_.store(pressed, std::memory_order_relaxed); }
    bool pressed() const { return pressed_.load(std::memory_order_relaxed); }

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    PinIndex signal_ = 0;
    LogicValue rail_ = LogicValue::Zero;
    std::atomic<bool> pressed_{false};
};

}  // namespace bw
