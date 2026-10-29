#include "runner/analyzer_consumer.hpp"

#include <algorithm>
#include <utility>

#include "runner/runner.hpp"

namespace bw {

namespace {

bool Running(AnalyzerPhase phase) {
    return phase == AnalyzerPhase::Armed || phase == AnalyzerPhase::Filling;
}

}  // namespace

void AnalyzerConsumer::configure(Runner& runner, AnalyzerConfig config) {
    {
        const std::scoped_lock lock(mutex_);
        pending_ = config;
    }
    generation_.fetch_add(1, std::memory_order_release);

    const AnalyzerPhase current = phase();
    if (config.armed && (config.arms != sent_.arms || Running(current))) {
        phase_.store(AnalyzerPhase::Armed, std::memory_order_release);
    } else if (!config.armed && Running(current)) {
        phase_.store(AnalyzerPhase::Idle, std::memory_order_release);
    }

    for (std::size_t c = 0; c < kChannels; ++c) {
        if (config.nets[c] != watched_[c] && config.nets[c]) {
            runner.watch(*config.nets[c]);
        }
    }
    for (std::size_t c = 0; c < kChannels; ++c) {
        if (config.nets[c] != watched_[c] && watched_[c]) {
            runner.unwatch(*watched_[c]);
        }
    }
    watched_ = config.nets;
    sent_ = std::move(config);
}

std::span<const NetId> AnalyzerConsumer::nets() const {
    if (generation_.load(std::memory_order_acquire) != netsGeneration_) {
        adoptNets();
    }
    return nets_;
}

void AnalyzerConsumer::adoptNets() const {
    netsGeneration_ = generation_.load(std::memory_order_acquire);
    const std::scoped_lock lock(mutex_);
    nets_.clear();
    masks_.clear();
    for (std::size_t c = 0; c < kChannels; ++c) {
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

void AnalyzerConsumer::adopt() {
    configGeneration_ = generation_.load(std::memory_order_acquire);
    const AnalyzerConfig previous = applied_;
    {
        const std::scoped_lock lock(mutex_);
        applied_ = pending_;
    }
    if (!applied_.armed) {
        state_.disarm();
    } else if (applied_.arms != previous.arms || Running(state_.phase())) {
        state_.arm(applied_.settings, applied_.labels, now_);
    }
}

void AnalyzerConsumer::powerOn() {
    now_ = 0;
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    state_.powerOn();
    phase_.store(state_.phase(), std::memory_order_release);
}

void AnalyzerConsumer::sample(NetId net, Sample sample) {
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    const auto found = std::ranges::find(nets_, net);
    if (found == nets_.end()) {
        return;
    }
    state_.change(sample.time, masks_[static_cast<std::size_t>(found - nets_.begin())],
                  sample.value);
}

void AnalyzerConsumer::advanceTo(Time time) {
    if (generation_.load(std::memory_order_acquire) != configGeneration_) {
        adopt();
    }
    state_.advanceTo(time);
    now_ = time;
    phase_.store(state_.phase(), std::memory_order_release);
    if (state_.capture().serial != published_) {
        published_ = state_.capture().serial;
        captures_.back() = state_.capture();
        captures_.publish();
    }
}

void AnalyzerConsumer::powerOff() {
    state_.powerOff();
    phase_.store(state_.phase(), std::memory_order_release);
}

}  // namespace bw
