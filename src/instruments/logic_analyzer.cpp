#include "instruments/logic_analyzer.hpp"

#include <algorithm>
#include <cctype>
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

std::optional<std::vector<TriggerCondition>> ParseBusPattern(std::string_view pattern,
                                                             std::size_t width) {
    const std::size_t digits = (width + 3) / 4;
    if (pattern.size() > digits) {
        return std::nullopt;
    }
    std::vector<TriggerCondition> conditions(width, TriggerCondition::Any);
    if (pattern.empty()) {
        return conditions;
    }
    for (std::size_t d = 0; d < digits; ++d) {  // d: nibble, 0 = least significant
        const std::size_t padding = digits - pattern.size();
        const std::size_t at = digits - 1 - d;
        const char digit = at < padding ? '0' : pattern[at - padding];
        if (digit == 'x' || digit == 'X') {
            continue;
        }
        int value = 0;
        if (digit >= '0' && digit <= '9') {
            value = digit - '0';
        } else if (digit >= 'a' && digit <= 'f') {
            value = digit - 'a' + 10;
        } else if (digit >= 'A' && digit <= 'F') {
            value = digit - 'A' + 10;
        } else {
            return std::nullopt;
        }
        for (std::size_t b = 0; b < 4; ++b) {
            const bool one = (value >> b & 1) != 0;
            const std::size_t bit = d * 4 + b;
            if (bit >= width) {
                if (one) {
                    return std::nullopt;
                }
                continue;
            }
            conditions[bit] = one ? TriggerCondition::High : TriggerCondition::Low;
        }
    }
    return conditions;
}

std::string FormatBusPattern(std::span<const TriggerCondition> conditions) {
    const std::size_t digits = (conditions.size() + 3) / 4;
    std::string text;
    for (std::size_t n = digits; n-- > 0;) {
        int value = 0, any = 0, levels = 0;
        const std::size_t bits = std::min<std::size_t>(4, conditions.size() - n * 4);
        for (std::size_t b = 0; b < bits; ++b) {
            switch (conditions[n * 4 + b]) {
            case TriggerCondition::Any:
                ++any;
                break;
            case TriggerCondition::High:
                value |= 1 << b;
                ++levels;
                break;
            case TriggerCondition::Low:
                ++levels;
                break;
            case TriggerCondition::Rise:
            case TriggerCondition::Fall:
                break;
            }
        }
        if (any == static_cast<int>(bits)) {
            text += 'x';
        } else if (levels == static_cast<int>(bits)) {
            text += "0123456789ABCDEF"[value];
        } else {
            text += '?';
        }
    }
    return text;
}

std::optional<NameStem> StemOf(std::string_view pin) {
    std::size_t end = pin.size();
    while (end > 0 && std::isdigit(static_cast<unsigned char>(pin[end - 1])) == 0) {
        --end;
    }
    if (end == 0) {
        return std::nullopt;
    }
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(pin[begin - 1])) != 0) {
        --begin;
    }
    std::uint32_t index = 0;
    for (std::size_t i = begin; i < end; ++i) {
        index = index * 10 + static_cast<std::uint32_t>(pin[i] - '0');
    }
    return NameStem{std::string(pin.substr(0, begin)) + "#" + std::string(pin.substr(end)), index};
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
    masks_ = {};
    for (std::size_t c = 0; c < kChannels; ++c) {
        const ChannelMask bit = ChannelMask{1} << c;
        switch (settings_.trigger[c]) {
        case TriggerCondition::Any:
            break;
        case TriggerCondition::Low:
            masks_.low |= bit;
            break;
        case TriggerCondition::High:
            masks_.high |= bit;
            break;
        case TriggerCondition::Rise:
            masks_.rise |= bit;
            break;
        case TriggerCondition::Fall:
            masks_.fall |= bit;
            break;
        }
    }
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

void AnalyzerState::change(Time time, ChannelMask channels, LogicValue value) {
    emitUntil(time, false);
    switch (value) {
    case LogicValue::Zero:
        current_.high &= ~channels;
        current_.unknown &= ~channels;
        break;
    case LogicValue::One:
        current_.high |= channels;
        current_.unknown &= ~channels;
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
    const ChannelMask edges = masks_.rise | masks_.fall;
    if (((masks_.low | masks_.high | edges) & now.unknown) != 0 || (masks_.low & now.high) != 0 ||
        (masks_.high & ~now.high) != 0) {
        return false;
    }
    if (edges == 0) {
        return true;
    }
    if (!previous_ || (edges & previous_->unknown) != 0) {
        return false;
    }
    const ChannelMask rose = ~previous_->high & now.high;
    const ChannelMask fell = previous_->high & ~now.high;
    return (masks_.rise & ~rose) == 0 && (masks_.fall & ~fell) == 0;
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
    std::vector<NetId> nets;  // distinct
    std::vector<ChannelMask> masks;
    for (std::size_t c = 0; c < kChannels; ++c) {
        if (!channels_[c]) {
            continue;
        }
        const auto found = std::ranges::find(nets, *channels_[c]);
        if (found == nets.end()) {
            nets.push_back(*channels_[c]);
            masks.push_back(ChannelMask{1} << c);
        } else {
            masks[static_cast<std::size_t>(found - nets.begin())] |= ChannelMask{1} << c;
        }
    }
    for (const auto& [i, sample] : sim_.recorder().inOrder(nets, fedUntil_)) {
        state_.change(sample.time, masks[i], sample.value);
    }
    state_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
