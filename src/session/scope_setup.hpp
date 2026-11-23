#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "instruments/oscilloscope.hpp"
#include "runner/scope_consumer.hpp"
#include "session/leads.hpp"

namespace bw {

// The Oscilloscope as the player set it up (Session::scope()): its Hooks and settings, kept
// across rebuilds; each build's ScopeConsumer configured from them.
class ScopeSetup {
public:
    explicit ScopeSetup(const SessionBuild& build) : build_(&build) {}

    void bind(ScopeConsumer& consumer);  // a new build: its consumer, configured

    // As AnalyzerSetup::clip(), with the Oscilloscope's Channels.
    std::optional<std::size_t> hook(std::string_view reference, std::string_view pin);
    void unhook(std::size_t channel);
    std::optional<std::size_t> hookAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kScopeChannels> hooks() const { return hooks_; }
    std::optional<double> rail(std::size_t channel) const;  // RailVolts of the Hook's Net
    std::optional<NetId> net(std::size_t channel) const { return config_.nets[channel]; }
    void setSettings(const ScopeSettings& settings);
    const ScopeSettings& settings() const { return config_.settings; }
    SweepKind status() const { return consumer_->status(); }
    TripleBuffer<Sweep>& sweeps() { return consumer_->sweeps(); }  // replaced on rebuild

private:
    void configure();  // config_ with this build's Nets, to the consumer

    const SessionBuild* build_;
    ScopeConsumer* consumer_ = nullptr;
    std::array<std::optional<PadLead>, kScopeChannels> hooks_;
    ScopeConfig config_;  // nets filled per build
};

}  // namespace bw
