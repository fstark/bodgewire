#include "parts/keyboard.hpp"

#include <utility>

#include "parts/connector.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

}  // namespace

std::optional<std::uint8_t> KeyboardCode(char32_t key) {
    if (key >= U'a' && key <= U'z') {
        return static_cast<std::uint8_t>(key - 0x20);
    }
    if (key >= 0x20 && key <= 0x5F) {
        return static_cast<std::uint8_t>(key);
    }
    if (key == U'\r' || key == U'\n') {
        return 0x0D;
    }
    if (key == U'\b') {
        return 0x08;
    }
    return std::nullopt;
}

Keyboard::Keyboard(std::string reference, Time strobe)
    : Component(std::move(reference), ConnectorPins("KEYBOARD")), strobe_(strobe) {
    Check(strobe >= 1'000'000, "keyboard: strobe must be at least 1 us");
}

bool Keyboard::type(std::uint8_t code) {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    if (head - tail_.load(std::memory_order_acquire) >= kKeyboardWaiting) {
        return false;
    }
    waiting_[head % kKeyboardWaiting] = code;
    head_.store(head + 1, std::memory_order_release);
    return true;
}

void Keyboard::dropWaiting() {
    dropBefore_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
}

void Keyboard::powerOn(PowerOnContext& ctx) {
    phase_ = Phase::Idle;
    nextAt_ = 0;
    ctx.setOutput(kStrobe, Zero);
}

void Keyboard::next(EvalContext& ctx, Phase phase, Time after) {
    phase_ = phase;
    nextAt_ = ctx.now() + after;
    ctx.wakeAfter(after);
}

void Keyboard::evaluate(EvalContext& ctx) {
    if (phase_ != Phase::Idle && ctx.now() < nextAt_) {
        return;  // a wake from type(), mid-key: the key waits its turn
    }
    switch (phase_) {
    case Phase::Setup:
        ctx.drive(kStrobe, One, kKeyboardDelay);
        next(ctx, Phase::High, strobe_);
        return;
    case Phase::High:
        ctx.drive(kStrobe, Zero, kKeyboardDelay);
        next(ctx, Phase::Gap, kKeyboardDelay + kKeyboardGap);  // 1 us after the fall
        return;
    case Phase::Gap:
        phase_ = Phase::Idle;
        break;
    case Phase::Idle:
        break;
    }
    std::uint32_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint32_t head = head_.load(std::memory_order_acquire);
    const std::uint32_t drop = dropBefore_.load(std::memory_order_acquire);
    if (static_cast<std::int32_t>(drop - tail) > 0) {
        tail = drop;  // wrap-safe: the counters only grow
    }
    if (tail == head) {
        tail_.store(tail, std::memory_order_release);
        return;
    }
    const std::uint8_t code = waiting_[tail % kKeyboardWaiting];
    tail_.store(tail + 1, std::memory_order_release);
    for (PinIndex bit = 0; bit < 7; ++bit) {
        ctx.drive(kD0 + bit, (code >> bit) & 1 ? One : Zero, kKeyboardDelay);
    }
    next(ctx, Phase::Setup, kKeyboardSetup);
}

}  // namespace bw
