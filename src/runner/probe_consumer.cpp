#include "runner/probe_consumer.hpp"

#include "runner/runner.hpp"

namespace bw {

namespace {

constexpr std::uint64_t kHigh = 1, kLow = 2, kPulse = 4, kValid = 8;

}  // namespace

void ProbeConsumer::touch(Runner& runner, std::optional<NetId> net) {
    if (net == touched_) {
        return;
    }
    target_.store(net ? static_cast<std::int64_t>(ToIndex(*net)) : -1, std::memory_order_release);
    if (net) {
        runner.watch(*net);
    }
    if (touched_) {
        runner.unwatch(*touched_);
    }
    touched_ = net;
}

ProbeLamps ProbeConsumer::lamps() const {
    const std::uint64_t p = published_.load(std::memory_order_acquire);
    if (!touched_ || (p & kValid) == 0 || (p >> 32) != ToIndex(*touched_)) {
        return {};
    }
    return {.high = (p & kHigh) != 0, .low = (p & kLow) != 0, .pulse = (p & kPulse) != 0};
}

std::span<const NetId> ProbeConsumer::nets() const {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t < 0) {
        return {};
    }
    nets_[0] = NetId{static_cast<std::uint32_t>(t)};
    return nets_;
}

void ProbeConsumer::powerOn() { state_.reset(); }

void ProbeConsumer::sample(NetId net, Sample sample) {
    if (static_cast<std::int64_t>(ToIndex(net)) != fedNet_) {
        state_.reset();
        fedNet_ = ToIndex(net);
    }
    state_.feed(sample);
}

void ProbeConsumer::advanceTo(Time time) {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t != fedNet_) {
        state_.reset();
        fedNet_ = t;
    }
    if (t < 0) {
        published_.store(0, std::memory_order_release);
        return;
    }
    const ProbeLamps lamps = state_.lamps(time);
    published_.store(static_cast<std::uint64_t>(t) << 32 | kValid | (lamps.high ? kHigh : 0) |
                         (lamps.low ? kLow : 0) | (lamps.pulse ? kPulse : 0),
                     std::memory_order_release);
}

void ProbeConsumer::powerOff() {
    state_.reset();
    published_.store(0, std::memory_order_release);
}

}  // namespace bw
