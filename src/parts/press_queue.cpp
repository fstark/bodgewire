#include "parts/press_queue.hpp"

namespace bw {

PressQueue::PressQueue(Time hold, Time gap) : hold_(hold), gap_(gap), waiting_(kMatrixWaiting) {}

bool PressQueue::push(MatrixPress press) {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    if (head - tail_.load(std::memory_order_acquire) >= kMatrixWaiting) {
        return false;
    }
    waiting_[head % kMatrixWaiting] =
        static_cast<std::uint16_t>(press.key | (press.shift ? kShiftBit : 0));
    head_.store(head + 1, std::memory_order_release);
    return true;
}

void PressQueue::dropWaiting() {
    dropBefore_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
}

void PressQueue::reset() {
    showing_.store(kNotShowing, std::memory_order_relaxed);
    phase_ = Phase::Idle;
    nextAt_ = 0;
}

std::optional<Time> PressQueue::step(Time now) {
    std::optional<Time> wake;
    // The press's own timing; a wake from outside mid-press changes nothing here.
    if (phase_ != Phase::Idle && now >= nextAt_) {
        if (phase_ == Phase::Hold) {
            phase_ = Phase::Gap;
            nextAt_ = now + gap_;
            wake = gap_;
        } else {
            phase_ = Phase::Idle;
        }
    }
    if (phase_ == Phase::Idle) {
        std::uint32_t tail = tail_.load(std::memory_order_relaxed);
        const std::uint32_t head = head_.load(std::memory_order_acquire);
        const std::uint32_t drop = dropBefore_.load(std::memory_order_acquire);
        if (static_cast<std::int32_t>(drop - tail) > 0) {
            tail = drop;  // wrap-safe: the counters only grow
        }
        if (tail != head) {
            const std::uint16_t code = waiting_[tail % kMatrixWaiting];
            ++tail;
            current_ = {static_cast<std::size_t>(code & ~kShiftBit), (code & kShiftBit) != 0};
            phase_ = Phase::Hold;
            nextAt_ = now + hold_;
            wake = hold_;
        }
        tail_.store(tail, std::memory_order_release);
    }
    showing_.store(phase_ == Phase::Hold
                       ? static_cast<std::uint32_t>(current_.key | (current_.shift ? kShiftBit : 0))
                       : kNotShowing,
                   std::memory_order_relaxed);
    return wake;
}

std::optional<MatrixPress> PressQueue::current() const {
    if (phase_ != Phase::Hold) {
        return std::nullopt;
    }
    return current_;
}

bool PressQueue::scripted(std::size_t key, std::size_t shift) const {
    const std::uint32_t showing = showing_.load(std::memory_order_relaxed);
    if (showing == kNotShowing) {
        return false;
    }
    return key == (showing & ~kShiftBit) || ((showing & kShiftBit) != 0 && key == shift);
}

}  // namespace bw
