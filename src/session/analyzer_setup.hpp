#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "instruments/logic_analyzer.hpp"
#include "runner/analyzer_consumer.hpp"
#include "session/leads.hpp"

namespace bw {

// Analyzer Channels read as one Bus value.
struct ChannelGroup {
    std::string name;                   // "U1 A"; hand-made: "group 1"
    std::vector<std::size_t> channels;  // bit 0 first
};

// One row of the analyzer's screen (P8i): a Channel Group, or a Clip in none.
struct AnalyzerRow {
    std::optional<std::size_t> group;  // index into groups()
    std::size_t channel = 0;           // a loose Clip's Channel; a group's first in the order
};

struct ClipGroupResult {
    std::size_t first;                 // a Channel of the clicked pin
    std::optional<std::size_t> group;  // nullopt: the pin had no stem partner, clipped alone
};

// The Logic Analyzer as the player set it up (Session::analyzer()): its Clips, Channel Groups and
// settings, kept across rebuilds; each build's AnalyzerConsumer configured from them. Without a
// Machine, nothing can be clipped.
class AnalyzerSetup {
public:
    explicit AnalyzerSetup(const SessionBuild& build) : build_(&build) {}

    // Lowest free Channel; nullopt without a Machine, unknown pad, or all Channels used. A new
    // Clip arms (P8i): its lane is empty until a Capture has it.
    std::optional<std::size_t> clip(std::string_view reference, std::string_view pin);
    void unclip(std::size_t channel);
    std::optional<std::size_t> clipAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kChannels> clips() const { return clips_; }
    std::optional<NetId> net(std::size_t channel) const { return config_.nets[channel]; }
    // The pin's group on its chip: the pins sharing its name stem (StemOf; not power or
    // no-connect), at least 2, each clipped in bit order to the lowest free Channel, those already
    // clipped moved into the group. A pin with no partner is clipped alone. Errors (reason text):
    // no Machine or unknown pin, not enough free Channels.
    std::expected<ClipGroupResult, std::string> clipGroup(std::string_view reference,
                                                          std::string_view pin);
    void unclipGroup(std::size_t group);  // unclips every member
    // Hand grouping of at least 2 clipped Channels, taken out of their groups; ordered by their
    // pins' StemOf index when all have one and they differ, else as given (first = most
    // significant). nullopt: fewer than 2, or a free Channel.
    std::optional<std::size_t> group(std::vector<std::size_t> channels);
    void ungroup(std::size_t group);  // members stay clipped with their conditions
    // Sets each member's condition from a bus pattern (ParseBusPattern); false: refused.
    bool setGroupTrigger(std::size_t group, std::string_view pattern);
    std::string groupTrigger(std::size_t group) const;  // FormatBusPattern of the members
    std::span<const ChannelGroup> groups() const { return groups_; }
    std::optional<std::size_t> groupOf(std::size_t channel) const;
    // The rows top to bottom (P8i): each clipped Channel in the order clipped (a Clip goes at the
    // bottom, an unclip closes its gap), a group's row where its first member is; kept across
    // rebuilds.
    std::vector<AnalyzerRow> rows() const;
    // The row at place `from` moved to place `to` (clamped), the others closing up; a group's
    // members move together.
    void move(std::size_t from, std::size_t to);
    const std::string& label(std::size_t channel) const;  // the Clip's label; "" if free
    // Whether `capture` recorded these Channels' present Clips: each one's label the same (P8i: a
    // Channel freed and clipped elsewhere is not what the Capture holds).
    bool inCapture(const Capture& capture, std::span<const std::size_t> channels) const;
    // A power-on (Session::powerOn, before the runner's): arms when something is clipped (P8i).
    void poweringOn();

    void setSettings(const AnalyzerSettings& settings);
    const AnalyzerSettings& settings() const { return config_.settings; }
    void arm();
    AnalyzerPhase phase() const { return consumer_->phase(); }
    TripleBuffer<Capture>& captures() { return consumer_->captures(); }  // replaced on rebuild

private:
    friend class Session;                   // its builds
    void bind(AnalyzerConsumer& consumer);  // a new build: its consumer, configured
    void beforeRebuild();  // a finished Capture is not re-armed by the next consumer
    std::optional<PadLead> padLead(std::string_view reference, std::string_view pin) const;
    void configure();                      // config_ with this build's Nets, to the consumer
    void leaveGroup(std::size_t channel);  // a group left with one member is erased

    const SessionBuild* build_;
    AnalyzerConsumer* consumer_ = nullptr;
    std::array<std::optional<PadLead>, kChannels> clips_;
    std::vector<std::size_t> order_;    // the clipped Channels, top to bottom (P8i)
    std::vector<ChannelGroup> groups_;  // disjoint; erasing one renumbers the later ones
    AnalyzerConfig config_;             // nets filled per build
};

}  // namespace bw
