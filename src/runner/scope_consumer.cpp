#include "runner/scope_consumer.hpp"

namespace bw {

void ScopeConsumer::configure(Runner& runner, ScopeConfig config) {
    {
        const std::scoped_lock lock(mutex_);
        pending_ = config;
        generation_.fetch_add(1, std::memory_order_release);
    }
    if (config != sent_) {
        status_.store(SweepKind::None, std::memory_order_release);
    }
    Rewatch(runner, sent_.nets, config.nets);
    sent_ = config;
}

std::span<const NetId> ScopeConsumer::nets() const {
    if (generation_.load(std::memory_order_acquire) != netsGeneration_) {
        adoptNets();
    }
    return channels_.nets();
}

void ScopeConsumer::adoptNets() const {
    const std::scoped_lock lock(mutex_);
    netsGeneration_ = generation_.load(std::memory_order_acquire);
    channels_.adopt(pending_.nets);
}

void ScopeConsumer::adopt() {
    const ScopeConfig previous = applied_;
    {
        const std::scoped_lock lock(mutex_);
        configGeneration_ = generation_.load(std::memory_order_acquire);
        applied_ = pending_;
    }
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (applied_.nets[c] != previous.nets[c]) {
            state_.clear(c, now_, applied_.nets[c].has_value());
        }
    }
    if (applied_ != previous) {
        state_.configure(applied_.settings, now_);
    }
}

void ScopeConsumer::powerOn() {
    now_ = 0;
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    state_.powerOn();
    state_.configure(applied_.settings, 0);
    status_.store(SweepKind::None, std::memory_order_release);
}

void ScopeConsumer::sample(NetId net, Sample sample) {
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    const std::uint8_t mask = channels_.maskOf(net);
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if ((mask >> c & 1) != 0) {
            state_.change(c, sample);
        }
    }
}

void ScopeConsumer::seed(NetId net, Sample sample) {
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    const std::uint8_t mask = channels_.maskOf(net);
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if ((mask >> c & 1) != 0) {
            state_.seed(c, sample);
        }
    }
}

void ScopeConsumer::advanceTo(Time time) {
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    state_.advanceTo(time);
    now_ = time;
    status_.store(state_.status(), std::memory_order_release);
    if (state_.sweep().serial != published_) {
        published_ = state_.sweep().serial;
        sweeps_.back() = state_.sweep();
        sweeps_.publish();
    }
}

void ScopeConsumer::powerOff() { status_.store(SweepKind::None, std::memory_order_release); }

}  // namespace bw
