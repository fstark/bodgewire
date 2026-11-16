#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "instruments/logic_analyzer.hpp"
#include "session/session.hpp"
#include "view/time_view.hpp"

namespace bw {

// Runs of equal value of some Channels of a Capture, from its first sampled point.
struct CaptureRuns {
    std::vector<std::uint32_t> start;    // point where run r begins; run r ends at start[r + 1]
    std::vector<std::uint32_t> value;    // bit b: channels[b]; 0 when unknown
    std::vector<std::uint8_t> unknown;   // a Channel is X
    std::size_t end = 0;                 // points in the Capture
    std::size_t at(double point) const;  // run holding `point` (clamped)
};

CaptureRuns RunsOf(const Capture& capture, const std::vector<std::size_t>& channels);

// What a Capture holds from its first sampled point, ps from its Trigger.
TimeRange RecordOf(const Capture& capture);

// The floating Logic Analyzer: settings, Channel Group and Channel rows, the last Capture's
// waveforms on one canvas under an overview and a time axis.
class AnalyzerWindow {
public:
    // `owner`: it has the focus (a gold frame). True when clicked: it takes the focus.
    bool draw(bool& open, bool owner, Session& session);
    // From bw_ui, for screenshots: the view and marker taken at the first Capture; a cursor
    // drawn whenever the mouse is not over the waveforms.
    void preset(std::optional<TimeView> view, std::optional<Time> marker,
                std::optional<Time> cursor);
    bool focused() const { return focused_; }  // last frame: its Esc is not the Board's

private:
    struct Row {
        std::optional<std::size_t> group;  // a group's row, or a member's
        std::size_t channel = 0;           // Channel rows and members
        bool member = false;
    };
    struct PatternEdit {
        std::array<char, 16> text{};
        bool refused = false;
    };

    std::vector<Row> rows(const Session& session) const;
    const CaptureRuns& runs(const std::vector<std::size_t>& channels);
    void zoom(Time anchor, int notches);

    std::optional<Capture> shown_;  // kept across rebuilds and power-off
    std::optional<Time> shownPeriod_;
    TimeView view_;
    std::optional<Time> marker_;  // ps from the Trigger
    float pressX_ = 0;            // canvas press, to tell a click from a drag
    bool moved_ = false;
    float wheelRest_ = 0;
    bool focused_ = false;
    std::optional<TimeView> presetView_;
    std::optional<Time> presetMarker_, presetCursor_;
    std::set<std::size_t> selected_;                        // Channels picked for a hand group
    std::set<std::string> expanded_;                        // group names shown with their members
    std::map<std::vector<std::size_t>, CaptureRuns> runs_;  // for shown_
    std::map<std::string, PatternEdit> patterns_;           // per group name
    std::string editing_;                                   // group whose pattern is active
};

}  // namespace bw
