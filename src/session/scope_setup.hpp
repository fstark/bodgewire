#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "runner/scope_consumer.hpp"
#include "session/leads.hpp"

namespace bw {

// The Oscilloscope as the player set it up (Session::scope()): its Hooks and settings, kept
// across rebuilds; each build's ScopeConsumer configured from them.
class ScopeSetup {
public:
    explicit ScopeSetup(const SessionBuild& build) : build_(&build) {}

    // As AnalyzerSetup::clip(), with the Oscilloscope's Channels. A Hook made while the
    // Trigger's source is not hooked takes the Trigger (P8h).
    std::optional<std::size_t> hook(std::string_view reference, std::string_view pin);
    // Its band gone; the Trigger's source, when it was this Channel, the top band's (P8h).
    void unhook(std::size_t channel);
    std::optional<std::size_t> hookAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kScopeChannels> hooks() const { return hooks_; }
    // The hooked Channels top to bottom on the screen (P8h): a Hook goes at the bottom, an
    // unhook closes its gap; kept across rebuilds, as the Hooks are.
    std::span<const std::size_t> order() const { return order_; }
    // The band at place `from` moved to place `to` (clamped), the others closing up.
    void move(std::size_t from, std::size_t to);
    std::optional<double> rail(std::size_t channel) const;  // RailVolts of the Hook's Net
    std::optional<NetId> net(std::size_t channel) const { return config_.nets[channel]; }
    void setSettings(const ScopeSettings& settings);
    const ScopeSettings& settings() const { return config_.settings; }
    SweepKind status() const { return consumer_->status(); }
    TripleBuffer<Sweep>& sweeps() { return consumer_->sweeps(); }  // replaced on rebuild

private:
    friend class Session;                // its builds
    void bind(ScopeConsumer& consumer);  // a new build: its consumer, configured
    void configure();                    // config_ with this build's Nets, to the consumer

    const SessionBuild* build_;
    ScopeConsumer* consumer_ = nullptr;
    std::array<std::optional<PadLead>, kScopeChannels> hooks_;
    std::vector<std::size_t> order_;
    ScopeConfig config_;  // nets filled per build
};

}  // namespace bw
