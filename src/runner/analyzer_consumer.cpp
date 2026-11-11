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

// The devices thread applies a configure() only at its next Sample or slice; until then its phase
// is stale (a power-off would show Idle right after Arm), so phase() shows what this one will do.
void AnalyzerConsumer::configure(Runner& runner, AnalyzerConfig config) {
    const AnalyzerPhase current = phase();
    if (config.armed && (config.arms != sent_.arms || Running(current))) {
        expected_ = AnalyzerPhase::Armed;
    } else if (!config.armed && Running(current)) {
        expected_ = AnalyzerPhase::Idle;
    } else {
        expected_ = current;
    }
    {
        const std::scoped_lock lock(mutex_);
        pending_ = config;
        sentGeneration_ = generation_.fetch_add(1, std::memory_order_release) + 1;
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

AnalyzerPhase AnalyzerConsumer::phase() const {
    const std::uint64_t published = phase_.load(std::memory_order_acquire);
    return published >> 8 == sentGeneration_ ? static_cast<AnalyzerPhase>(published & 0xff)
                                             : expected_;
}

void AnalyzerConsumer::publishPhase() {
    phase_.store(configGeneration_ << 8 | static_cast<std::uint64_t>(state_.phase()),
                 std::memory_order_release);
}

std::span<const NetId> AnalyzerConsumer::nets() const {
    if (generation_.load(std::memory_order_acquire) != netsGeneration_) {
        adoptNets();
    }
    return nets_;
}

void AnalyzerConsumer::adoptNets() const {
    const std::scoped_lock lock(mutex_);
    netsGeneration_ = generation_.load(std::memory_order_acquire);
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
    const AnalyzerConfig previous = applied_;
    {
        const std::scoped_lock lock(mutex_);
        configGeneration_ = generation_.load(std::memory_order_acquire);
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
    publishPhase();
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
    publishPhase();
    if (state_.capture().serial != published_) {
        published_ = state_.capture().serial;
        captures_.back() = state_.capture();
        captures_.publish();
    }
}

void AnalyzerConsumer::powerOff() {
    state_.powerOff();
    publishPhase();
}

}  // namespace bw
