#include "session/scope_setup.hpp"

#include <algorithm>
#include <utility>

namespace bw {

void ScopeSetup::bind(ScopeConsumer& consumer) {
    consumer_ = &consumer;
    configure();
}

void ScopeSetup::configure() {
    if (build_->machine == nullptr || consumer_ == nullptr) {
        return;
    }
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        config_.nets[c] = hooks_[c] ? PinNet(*build_->circuit, hooks_[c]->reference, hooks_[c]->pin)
                                    : std::nullopt;
    }
    consumer_->configure(*build_->runner, config_);
}

std::optional<std::size_t> ScopeSetup::hook(std::string_view reference, std::string_view pin) {
    auto lead = build_->machine ? PadLeadOf(*build_->machine, reference, pin) : std::nullopt;
    if (!lead) {
        return std::nullopt;
    }
    if (const auto channel = hookAt(reference, lead->pin)) {
        return channel;
    }
    const auto free = std::ranges::find_if(hooks_, [](const auto& h) { return !h; });
    if (free == hooks_.end()) {
        return std::nullopt;
    }
    *free = std::move(*lead);
    const auto channel = static_cast<std::size_t>(free - hooks_.begin());
    if (!hooks_[config_.settings.source] || config_.settings.source == channel) {
        config_.settings.source = channel;  // the first Hook, or the source's gone: it triggers
    }
    order_.push_back(channel);
    configure();
    return channel;
}

void ScopeSetup::unhook(std::size_t channel) {
    if (!hooks_[channel]) {
        return;
    }
    hooks_[channel].reset();
    std::erase(order_, channel);
    if (config_.settings.source == channel && !order_.empty()) {
        config_.settings.source = order_.front();
    }
    configure();
}

void ScopeSetup::move(std::size_t from, std::size_t to) {
    if (from >= order_.size()) {
        return;
    }
    to = std::min(to, order_.size() - 1);
    const std::size_t channel = order_[from];
    order_.erase(order_.begin() + static_cast<std::ptrdiff_t>(from));
    order_.insert(order_.begin() + static_cast<std::ptrdiff_t>(to), channel);
}

std::optional<std::size_t> ScopeSetup::hookAt(std::string_view reference,
                                              std::string_view pin) const {
    for (std::size_t c = 0; c < kScopeChannels; ++c) {
        if (hooks_[c] && hooks_[c]->reference == reference && hooks_[c]->pin == pin) {
            return c;
        }
    }
    return std::nullopt;
}

std::optional<double> ScopeSetup::rail(std::size_t channel) const {
    return config_.nets[channel] ? RailVolts(*build_->circuit, *config_.nets[channel])
                                 : std::nullopt;
}

void ScopeSetup::setSettings(const ScopeSettings& settings) {
    config_.settings = settings;
    configure();
}

}  // namespace bw
