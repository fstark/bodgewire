#include "runner/scope_consumer.hpp"

#include <algorithm>

#include "runner/runner.hpp"

namespace bw {

void ScopeConsumer::configure(Runner& runner, ScopeConfig config) {
    {
        const std::scoped_lock lock(mutex_);
        pending_ = config;
    }
    generation_.fetch_add(1, std::memory_order_release);
    if (config != sent_) {
        status_.store(SweepKind::None, std::memory_order_release);
    }
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (config.nets[c] != sent_.nets[c] && config.nets[c]) {
            runner.watch(*config.nets[c]);
        }
    }
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (config.nets[c] != sent_.nets[c] && sent_.nets[c]) {
            runner.unwatch(*sent_.nets[c]);
        }
    }
    sent_ = config;
}

std::span<const NetId> ScopeConsumer::nets() const {
    if (generation_.load(std::memory_order_acquire) != netsGeneration_) {
        adoptNets();
    }
    return nets_;
}

void ScopeConsumer::adoptNets() const {
    netsGeneration_ = generation_.load(std::memory_order_acquire);
    const std::scoped_lock lock(mutex_);
    nets_.clear();
    masks_.clear();
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        const auto& net = pending_.nets[c];
        if (!net) {
            continue;
        }
        const auto bit = static_cast<std::uint8_t>(1u << c);
        const auto found = std::ranges::find(nets_, *net);
        if (found == nets_.end()) {
            nets_.push_back(*net);
            masks_.push_back(bit);
        } else {
            masks_[static_cast<std::size_t>(found - nets_.begin())] |= bit;
        }
    }
}

void ScopeConsumer::adopt() {
    configGeneration_ = generation_.load(std::memory_order_acquire);
    const ScopeConfig previous = applied_;
    {
        const std::scoped_lock lock(mutex_);
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

std::uint8_t ScopeConsumer::maskOf(NetId net) const {
    const auto found = std::ranges::find(nets_, net);
    return found == nets_.end() ? 0 : masks_[static_cast<std::size_t>(found - nets_.begin())];
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
    const std::uint8_t mask = maskOf(net);
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
    const std::uint8_t mask = maskOf(net);
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
