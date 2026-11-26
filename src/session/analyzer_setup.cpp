#include "session/analyzer_setup.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

namespace bw {

namespace {

// The bus a Pin belongs to and its bit: a PinGroup the Part declares (QA-QD), else the digits in
// its name (A0-A15, StemOf). Power and unconnected Pins belong to none.
std::optional<NameStem> BusBitOf(const Component& part, std::size_t pin) {
    const PinSpec& spec = part.pins()[pin];
    if (spec.kind == PinKind::Power || spec.kind == PinKind::NoConnect) {
        return std::nullopt;
    }
    for (const PinGroup& group : part.pinGroups()) {
        if (const auto bit = std::ranges::find(group.pins, pin); bit != group.pins.end()) {
            return NameStem{group.name, static_cast<std::uint32_t>(bit - group.pins.begin())};
        }
    }
    return StemOf(spec.name);
}

}  // namespace

std::optional<PadLead> AnalyzerSetup::padLead(std::string_view reference,
                                              std::string_view pin) const {
    return build_->machine ? PadLeadOf(*build_->machine, reference, pin) : std::nullopt;
}

void AnalyzerSetup::bind(AnalyzerConsumer& consumer) {
    consumer_ = &consumer;
    configure();
}

void AnalyzerSetup::beforeRebuild() {
    const AnalyzerPhase now = phase();
    if (now != AnalyzerPhase::Armed && now != AnalyzerPhase::Filling) {
        config_.armed = false;  // the new consumer would re-arm a finished Capture
    }
}

void AnalyzerSetup::configure() {
    if (build_->machine == nullptr || consumer_ == nullptr) {
        return;
    }
    for (std::size_t c = 0; c < kChannels; ++c) {
        config_.nets[c] = clips_[c] ? PinNet(*build_->circuit, clips_[c]->reference, clips_[c]->pin)
                                    : std::nullopt;
        config_.labels[c] = clips_[c] ? clips_[c]->label : "";
    }
    consumer_->configure(*build_->runner, config_);
}

std::optional<std::size_t> AnalyzerSetup::clip(std::string_view reference, std::string_view pin) {
    auto lead = padLead(reference, pin);
    if (!lead) {
        return std::nullopt;
    }
    if (const auto channel = clipAt(reference, lead->pin)) {
        return channel;
    }
    const auto free = std::ranges::find_if(clips_, [](const auto& c) { return !c; });
    if (free == clips_.end()) {
        return std::nullopt;
    }
    *free = std::move(*lead);
    const auto channel = static_cast<std::size_t>(free - clips_.begin());
    order_.push_back(channel);
    arm();
    return channel;
}

void AnalyzerSetup::unclip(std::size_t channel) {
    leaveGroup(channel);
    clips_[channel].reset();
    std::erase(order_, channel);
    config_.settings.trigger[channel] = TriggerCondition::Any;  // a free Channel reads X
    configure();
}

std::optional<std::size_t> AnalyzerSetup::clipAt(std::string_view reference,
                                                 std::string_view pin) const {
    for (std::size_t c = 0; c < kChannels; ++c) {
        if (clips_[c] && clips_[c]->reference == reference && clips_[c]->pin == pin) {
            return c;
        }
    }
    return std::nullopt;
}

std::expected<ClipGroupResult, std::string> AnalyzerSetup::clipGroup(std::string_view reference,
                                                                     std::string_view pin) {
    const auto lead = padLead(reference, pin);
    if (!lead) {
        return std::unexpected("no such pin");
    }
    std::optional<NameStem> stem;  // a resistor's, a capacitor's pad: none, clipped alone
    std::vector<std::pair<std::uint32_t, std::string>> members;  // bit, pin number
    if (lead->chip) {
        const Component& part =
            *build_->machine->designCircuit().components.find(reference)->second;
        stem = BusBitOf(part, lead->index);
        const auto pins = part.pins();
        for (std::size_t p = 0; stem && p < pins.size(); ++p) {
            const auto other = BusBitOf(part, p);
            if (other && other->stem == stem->stem) {
                members.emplace_back(other->index, std::to_string(p + 1));
            }
        }
    }
    if (members.size() < 2) {
        const auto channel = clip(reference, lead->pin);
        if (!channel) {
            return std::unexpected(std::format("all {} channels used", kChannels));
        }
        return ClipGroupResult{*channel, std::nullopt};
    }
    std::ranges::sort(members);
    std::string name = stem->stem;
    std::erase(name, '#');
    name = std::format("{} {}", reference, name);
    const auto needed = static_cast<std::size_t>(std::ranges::count_if(
        members, [&](const auto& m) { return !clipAt(reference, m.second).has_value(); }));
    const auto free =
        static_cast<std::size_t>(std::ranges::count_if(clips_, [](const auto& c) { return !c; }));
    if (needed > free) {
        return std::unexpected(
            std::format("group {} needs {} channels, {} free", name, needed, free));
    }
    std::vector<std::size_t> channels;
    for (const auto& [index, number] : members) {
        channels.push_back(*clip(reference, number));
    }
    for (const std::size_t channel : channels) {
        leaveGroup(channel);
    }
    groups_.push_back({std::move(name), std::move(channels)});
    return ClipGroupResult{*clipAt(reference, lead->pin), groups_.size() - 1};
}

void AnalyzerSetup::unclipGroup(std::size_t group) {
    const std::vector<std::size_t> channels = std::move(groups_[group].channels);
    groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(group));
    for (const std::size_t channel : channels) {
        clips_[channel].reset();
        std::erase(order_, channel);
        config_.settings.trigger[channel] = TriggerCondition::Any;
    }
    configure();
}

std::optional<std::size_t> AnalyzerSetup::group(std::vector<std::size_t> channels) {
    if (channels.size() < 2) {
        return std::nullopt;
    }
    std::vector<std::pair<std::uint32_t, std::size_t>> stems;  // stem index, Channel
    for (const std::size_t channel : channels) {
        if (channel >= kChannels || !clips_[channel] ||
            std::ranges::count(channels, channel) != 1) {
            return std::nullopt;
        }
        const PadLead& clip = *clips_[channel];
        const auto stem =
            clip.chip ? BusBitOf(*build_->machine->designCircuit().components.at(clip.reference),
                                 clip.index)
                      : std::nullopt;
        if (stem) {
            stems.emplace_back(stem->index, channel);
        }
    }
    std::ranges::sort(stems);
    const bool byStem =
        stems.size() == channels.size() &&
        std::ranges::adjacent_find(stems, {}, &std::pair<std::uint32_t, std::size_t>::first) ==
            stems.end();
    if (byStem) {
        channels.clear();
        for (const auto& [index, channel] : stems) {
            channels.push_back(channel);
        }
    } else {
        std::ranges::reverse(channels);  // given most significant first
    }
    for (const std::size_t channel : channels) {
        leaveGroup(channel);
    }
    int n = 1;
    while (std::ranges::any_of(
        groups_, [&](const ChannelGroup& g) { return g.name == std::format("group {}", n); })) {
        ++n;
    }
    groups_.push_back({std::format("group {}", n), std::move(channels)});
    return groups_.size() - 1;
}

void AnalyzerSetup::ungroup(std::size_t group) {
    groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(group));
}

bool AnalyzerSetup::setGroupTrigger(std::size_t group, std::string_view pattern) {
    const auto& channels = groups_[group].channels;
    const auto conditions = ParseBusPattern(pattern, channels.size());
    if (!conditions) {
        return false;
    }
    for (std::size_t b = 0; b < channels.size(); ++b) {
        config_.settings.trigger[channels[b]] = (*conditions)[b];
    }
    configure();
    return true;
}

std::string AnalyzerSetup::groupTrigger(std::size_t group) const {
    std::vector<TriggerCondition> conditions;
    for (const std::size_t channel : groups_[group].channels) {
        conditions.push_back(config_.settings.trigger[channel]);
    }
    return FormatBusPattern(conditions);
}

std::optional<std::size_t> AnalyzerSetup::groupOf(std::size_t channel) const {
    for (std::size_t g = 0; g < groups_.size(); ++g) {
        if (std::ranges::contains(groups_[g].channels, channel)) {
            return g;
        }
    }
    return std::nullopt;
}

void AnalyzerSetup::leaveGroup(std::size_t channel) {
    const auto group = groupOf(channel);
    if (!group) {
        return;
    }
    std::erase(groups_[*group].channels, channel);
    if (groups_[*group].channels.size() < 2) {
        groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(*group));
    }
}

void AnalyzerSetup::setSettings(const AnalyzerSettings& settings) {
    config_.settings = settings;
    configure();
}

void AnalyzerSetup::arm() {
    ++config_.arms;
    config_.armed = true;
    configure();
}

void AnalyzerSetup::poweringOn() {
    if (!order_.empty()) {
        arm();
    }
}

std::vector<AnalyzerRow> AnalyzerSetup::rows() const {
    std::vector<AnalyzerRow> rows;
    std::vector<bool> met(groups_.size(), false);
    for (const std::size_t channel : order_) {
        const auto group = groupOf(channel);
        if (!group) {
            rows.push_back({std::nullopt, channel});
        } else if (!met[*group]) {
            met[*group] = true;
            rows.push_back({group, channel});
        }
    }
    return rows;
}

void AnalyzerSetup::move(std::size_t from, std::size_t to) {
    const std::vector<AnalyzerRow> before = rows();
    if (from >= before.size()) {
        return;
    }
    to = std::min(to, before.size() - 1);
    if (from == to) {
        return;
    }
    const auto channelsOf = [&](const AnalyzerRow& row) {
        return row.group ? groups_[*row.group].channels : std::vector{row.channel};
    };
    std::vector<std::vector<std::size_t>> blocks;  // each row's Channels in their order
    for (const AnalyzerRow& row : before) {
        std::vector<std::size_t> block;
        const std::vector<std::size_t> members = channelsOf(row);
        for (const std::size_t channel : order_) {
            if (std::ranges::contains(members, channel)) {
                block.push_back(channel);
            }
        }
        blocks.push_back(std::move(block));
    }
    std::vector<std::size_t> moved = std::move(blocks[from]);
    blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(from));
    blocks.insert(blocks.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    order_.clear();
    for (const auto& block : blocks) {
        order_.insert(order_.end(), block.begin(), block.end());
    }
}

const std::string& AnalyzerSetup::label(std::size_t channel) const {
    static const std::string kNone;
    return clips_[channel] ? clips_[channel]->label : kNone;
}

bool AnalyzerSetup::inCapture(const Capture& capture, std::span<const std::size_t> channels) const {
    return std::ranges::all_of(channels, [&](std::size_t c) {
        return clips_[c] && capture.labels[c] == clips_[c]->label;
    });
}

}  // namespace bw
