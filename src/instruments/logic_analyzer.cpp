#include "instruments/logic_analyzer.hpp"

#include <algorithm>
#include <utility>

namespace bw {

std::size_t PreTriggerPoints(TriggerPosition position) {
    switch (position) {
    case TriggerPosition::Early:
        return kDepth * 10 / 100;
    case TriggerPosition::Middle:
        return kDepth * 50 / 100;
    case TriggerPosition::Late:
        return kDepth * 90 / 100;
    }
    return 0;
}

std::optional<TriggerCondition> ParseTrigger(std::string_view text) {
    if (text == "0") {
        return TriggerCondition::Low;
    }
    if (text == "1") {
        return TriggerCondition::High;
    }
    if (text == "r") {
        return TriggerCondition::Rise;
    }
    if (text == "f") {
        return TriggerCondition::Fall;
    }
    return std::nullopt;
}

void AnalyzerState::powerOn() {
    current_ = {};
    previous_.reset();
    if (phase_ == AnalyzerPhase::Armed || phase_ == AnalyzerPhase::Filling) {
        arm(settings_, labels_, 0);
    }
}

void AnalyzerState::powerOff() {
    if (phase_ == AnalyzerPhase::Filling) {
        phase_ = AnalyzerPhase::Armed;
    }
}

void AnalyzerState::arm(const AnalyzerSettings& settings, std::array<std::string, kChannels> labels,
                        Time now) {
    settings_ = settings;
    labels_ = std::move(labels);
    next_ = armedAt_ = (now + settings_.period - 1) / settings_.period * settings_.period;
    emitted_ = 0;
    previous_.reset();
    std::ranges::fill(ring_, ChannelBits{});
    phase_ = AnalyzerPhase::Armed;
}

void AnalyzerState::disarm() {
    if (phase_ == AnalyzerPhase::Armed || phase_ == AnalyzerPhase::Filling) {
        phase_ = AnalyzerPhase::Idle;
    }
}

void AnalyzerState::change(Time time, std::uint8_t channels, LogicValue value) {
    emitUntil(time, false);
    switch (value) {
    case LogicValue::Zero:
        current_.high &= static_cast<std::uint8_t>(~channels);
        current_.unknown &= static_cast<std::uint8_t>(~channels);
        break;
    case LogicValue::One:
        current_.high |= channels;
        current_.unknown &= static_cast<std::uint8_t>(~channels);
        break;
    case LogicValue::Z:
    case LogicValue::X:
        current_.unknown |= channels;
        break;
    }
}

void AnalyzerState::advanceTo(Time end) { emitUntil(end, true); }

void AnalyzerState::emitUntil(Time limit, bool inclusive) {
    if (phase_ != AnalyzerPhase::Armed && phase_ != AnalyzerPhase::Filling) {
        return;
    }
    const Time last = inclusive ? limit : limit - 1;
    if (last < next_) {
        return;
    }
    const auto count = static_cast<std::uint64_t>((last - next_) / settings_.period + 1);
    next_ += static_cast<Time>(count) * settings_.period;
    emit(count);
}

void AnalyzerState::emit(std::uint64_t count) {
    while (count > 0) {
        if (phase_ == AnalyzerPhase::Armed && triggers(current_)) {
            triggerPoint_ = emitted_;
            remaining_ = kDepth - PreTriggerPoints(settings_.position);
            phase_ = AnalyzerPhase::Filling;
        }
        const std::uint64_t k =
            phase_ == AnalyzerPhase::Filling ? std::min(count, remaining_) : count;
        write(k);
        emitted_ += k;
        count -= k;
        previous_ = current_;
        if (phase_ == AnalyzerPhase::Filling) {
            remaining_ -= k;
            if (remaining_ == 0) {
                freeze();
                return;
            }
        } else {
            return;  // Armed: the rest of the run cannot trigger
        }
    }
}

void AnalyzerState::write(std::uint64_t count) {
    const std::uint64_t n = std::min<std::uint64_t>(count, kDepth);
    const std::size_t begin = (emitted_ + count - n) % kDepth;
    const std::size_t head = std::min<std::size_t>(n, kDepth - begin);
    const auto ring = ring_.begin();
    std::fill(ring + static_cast<std::ptrdiff_t>(begin),
              ring + static_cast<std::ptrdiff_t>(begin + head), current_);
    std::fill(ring, ring + static_cast<std::ptrdiff_t>(n - head), current_);
}

bool AnalyzerState::triggers(ChannelBits now) const {
    for (std::size_t c = 0; c < kChannels; ++c) {
        const auto bit = static_cast<std::uint8_t>(1u << c);
        const bool known = (now.unknown & bit) == 0;
        const bool high = (now.high & bit) != 0;
        const bool wasKnown = previous_ && (previous_->unknown & bit) == 0;
        const bool wasHigh = previous_ && (previous_->high & bit) != 0;
        bool ok = true;
        switch (settings_.trigger[c]) {
        case TriggerCondition::Any:
            break;
        case TriggerCondition::Low:
            ok = known && !high;
            break;
        case TriggerCondition::High:
            ok = known && high;
            break;
        case TriggerCondition::Rise:
            ok = known && wasKnown && !wasHigh && high;
            break;
        case TriggerCondition::Fall:
            ok = known && wasKnown && wasHigh && !high;
            break;
        }
        if (!ok) {
            return false;
        }
    }
    return true;
}

void AnalyzerState::freeze() {
    const std::size_t pre = PreTriggerPoints(settings_.position);
    const auto oldest = static_cast<std::ptrdiff_t>(emitted_ % kDepth);
    capture_.points.assign(ring_.begin() + oldest, ring_.end());
    capture_.points.insert(capture_.points.end(), ring_.begin(), ring_.begin() + oldest);
    capture_.period = settings_.period;
    capture_.trigger = pre;
    capture_.first = pre > triggerPoint_ ? pre - triggerPoint_ : 0;
    capture_.triggerTime = armedAt_ + static_cast<Time>(triggerPoint_) * settings_.period;
    capture_.labels = labels_;
    ++capture_.serial;
    phase_ = AnalyzerPhase::Captured;
}

LogicAnalyzer::LogicAnalyzer(Simulator& sim, std::array<std::optional<NetId>, kChannels> channels)
    : sim_(sim), channels_(channels) {
    for (const auto& net : channels_) {
        if (net) {
            sim_.record(*net);
        }
    }
}

void LogicAnalyzer::arm(const AnalyzerSettings& settings) { state_.arm(settings, {}, sim_.now()); }

void LogicAnalyzer::update() {
    struct Change {
        Time time;
        std::uint8_t channels;
        LogicValue value;
    };
    std::vector<Change> changes;
    std::vector<NetId> done;
    for (const auto& net : channels_) {
        if (!net || std::ranges::find(done, *net) != done.end()) {
            continue;
        }
        done.push_back(*net);
        std::uint8_t mask = 0;
        for (std::size_t c = 0; c < kChannels; ++c) {
            if (channels_[c] == net) {
                mask |= static_cast<std::uint8_t>(1u << c);
            }
        }
        for (const Sample sample : sim_.recorder().samples(*net)) {
            if (sample.time > fedUntil_) {
                changes.push_back({sample.time, mask, sample.value});
            }
        }
    }
    std::ranges::stable_sort(changes, {}, &Change::time);
    for (const Change& change : changes) {
        state_.change(change.time, change.channels, change.value);
    }
    state_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
