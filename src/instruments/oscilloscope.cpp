#include "instruments/oscilloscope.hpp"

#include <algorithm>

namespace bw {

void ScopeState::powerOn() {
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        history_[c] = {{0, LogicValue::X}};
        last_[c] = LogicValue::X;
    }
    now_ = 0;
    sweeping_ = false;
    waitFrom_ = 0;
    pending_.reset();
    status_ = SweepKind::None;
}

void ScopeState::configure(const ScopeSettings& settings, Time now) {
    settings_ = settings;
    settings_.delay = std::clamp<Time>(settings_.delay, 0, kMaxDelayDivisions * settings_.timebase);
    sweeping_ = false;
    pending_.reset();
    waitFrom_ = now;
    status_ = SweepKind::None;
}

void ScopeState::clear(std::size_t channel, Time now, bool hooked) {
    history_[channel] = {{now, LogicValue::X}};
    last_[channel] = LogicValue::X;
    hooked_[channel] = hooked;
}

void ScopeState::change(std::size_t channel, Sample sample) {
    catchUp(sample.time, false);
    if (!sweeping_ && channel == settings_.source && sample.time > waitFrom_) {
        const LogicValue from =
            settings_.edge == ScopeEdge::Rise ? LogicValue::Zero : LogicValue::One;
        const LogicValue to =
            settings_.edge == ScopeEdge::Rise ? LogicValue::One : LogicValue::Zero;
        if (last_[channel] == from && sample.value == to) {
            startSweep(sample.time, SweepKind::Triggered);
        }
    }
    append(channel, sample);
    last_[channel] = sample.value;
    now_ = std::max(now_, sample.time);
}

void ScopeState::seed(std::size_t channel, Sample sample) {
    append(channel, {std::max(sample.time, now_), sample.value, sample.floating});
    last_[channel] = sample.value;
}

void ScopeState::append(std::size_t channel, Sample sample) {
    auto& history = history_[channel];
    if (!history.empty() && history.back().time == sample.time) {
        history.back() = sample;  // same instant: the later value wins
    } else {
        history.push_back(sample);
    }
}

void ScopeState::advanceTo(Time end) {
    catchUp(end, true);
    now_ = std::max(now_, end);
    if (pending_) {
        materialize();
        pending_.reset();
    }
    prune(sweeping_ ? start_ : end - settings_.timebase);
}

Time ScopeState::wait() const { return std::max(kAutoWait, kDivisions * settings_.timebase); }

void ScopeState::catchUp(Time time, bool inclusive) {
    const auto before = [&](Time t) { return inclusive ? t <= time : t < time; };
    for (;;) {
        if (sweeping_ && before(end_)) {
            pending_ = Pending{start_, kind_};
            status_ = kind_;
            sweeping_ = false;
            waitFrom_ = end_;
        } else if (!sweeping_ && settings_.mode == ScopeMode::Auto && before(waitFrom_ + wait())) {
            startSweep(waitFrom_ + wait(), SweepKind::Auto);
        } else {
            break;
        }
    }
}

void ScopeState::startSweep(Time trigger, SweepKind kind) {
    sweeping_ = true;
    start_ = trigger + settings_.delay - settings_.timebase;
    end_ = start_ + kDivisions * settings_.timebase;
    kind_ = kind;
}

void ScopeState::materialize() {
    const Time start = pending_->start;
    const Time end = start + kDivisions * settings_.timebase;
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        auto& out = sweep_.channels[c];
        out.clear();
        if (!hooked_[c]) {
            continue;
        }
        const auto& history = history_[c];
        auto it = std::ranges::upper_bound(history, start, {}, &Sample::time);
        Sample first{start, LogicValue::X};
        if (it != history.begin()) {
            const Sample& at = *std::prev(it);
            first = {start, at.value, at.floating};
        }
        out.push_back(first);
        for (; it != history.end() && it->time <= end; ++it) {
            out.push_back(*it);
        }
    }
    ++sweep_.serial;
    sweep_.kind = pending_->kind;
    sweep_.timebase = settings_.timebase;
    sweep_.delay = settings_.delay;
    sweep_.start = start;
}

void ScopeState::prune(Time keepFrom) {
    for (auto& history : history_) {
        const auto it = std::ranges::upper_bound(history, keepFrom, {}, &Sample::time);
        if (it == history.begin()) {
            continue;
        }
        const auto i = static_cast<std::size_t>(std::prev(it) - history.begin());
        if (i > 0 && i >= history.size() / 2) {
            history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

Oscilloscope::Oscilloscope(Simulator& sim,
                           std::array<std::optional<NetId>, kScopeChannels> channels,
                           const ScopeSettings& settings)
    : sim_(sim), channels_(channels) {
    for (const auto& net : channels_) {
        if (net) {
            sim_.record(*net);
        }
    }
    state_.powerOn();
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        state_.clear(c, sim_.now(), channels_[c].has_value());
    }
    state_.configure(settings, sim_.now());
}

void Oscilloscope::update() {
    std::vector<NetId> nets;
    std::vector<std::size_t> channelOf;  // per nets entry
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (channels_[c]) {
            nets.push_back(*channels_[c]);
            channelOf.push_back(c);
        }
    }
    for (const auto& [i, sample] : sim_.recorder().inOrder(nets, fedUntil_)) {
        state_.change(channelOf[i], sample);
    }
    state_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

std::optional<std::size_t> TimebaseIndex(Time timebase) {
    const auto it = std::ranges::find(kTimebases, timebase);
    if (it == kTimebases.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - kTimebases.begin());
}

}  // namespace bw
