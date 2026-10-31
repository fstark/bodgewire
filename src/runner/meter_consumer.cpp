#include "runner/meter_consumer.hpp"

#include <bit>

#include "runner/runner.hpp"

namespace bw {

void MeterConsumer::touch(Runner& runner, std::optional<NetId> net) {
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

std::optional<double> MeterConsumer::reading() const {
    const std::uint64_t p = published_.load(std::memory_order_acquire);
    if (!touched_ || (p >> 32) != std::uint64_t{ToIndex(*touched_)} + 1) {
        return std::nullopt;
    }
    return std::bit_cast<float>(static_cast<std::uint32_t>(p));
}

std::span<const NetId> MeterConsumer::nets() const {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t < 0) {
        return {};
    }
    nets_[0] = NetId{static_cast<std::uint32_t>(t)};
    return nets_;
}

void MeterConsumer::powerOn() { state_.reset(); }

void MeterConsumer::sample(NetId net, Sample sample) {
    if (static_cast<std::int64_t>(ToIndex(net)) != fedNet_) {
        state_.reset();
        fedNet_ = ToIndex(net);
    }
    state_.feed(sample);
}

void MeterConsumer::advanceTo(Time time) {
    const std::int64_t t = target_.load(std::memory_order_acquire);
    if (t != fedNet_) {
        state_.reset();
        fedNet_ = t;
    }
    state_.advanceTo(time);
    const auto reading = state_.reading();
    if (t < 0 || !reading) {
        published_.store(0, std::memory_order_release);
        return;
    }
    published_.store(static_cast<std::uint64_t>(t + 1) << 32 |
                         std::bit_cast<std::uint32_t>(static_cast<float>(*reading)),
                     std::memory_order_release);
}

void MeterConsumer::powerOff() {
    state_.reset();
    published_.store(0, std::memory_order_release);
}

}  // namespace bw
