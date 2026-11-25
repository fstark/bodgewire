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
#include "ui/controls.hpp"
#include "ui/gestures.hpp"
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
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
    // From bw_ui, for screenshots: the view and marker taken at the first Capture; a cursor
    // drawn whenever the mouse is not over the waveforms.
    void preset(std::optional<TimeView> view, std::optional<Time> marker,
                std::optional<Time> cursor);
    // The pin / bus switch (P8f) is on bus: a click on a pin clips or unclips its whole bus, as
    // a Shift-click does.
    bool bus() const { return bus_; }

private:
    struct Row {
        std::optional<std::size_t> group;  // a group's row, or a member's
        std::size_t channel = 0;           // Channel rows and members
        bool member = false;
    };

    std::vector<Row> rows(const Session& session) const;
    const CaptureRuns& runs(const std::vector<std::size_t>& channels);
    void zoom(Time anchor, int notches);

    std::optional<Capture> shown_;  // kept across rebuilds and power-off
    std::optional<Time> shownPeriod_;
    TimeView view_;
    std::optional<Time> marker_;  // ps from the Trigger
    TimeCanvas canvas_;
    DragScroll labelScroll_;   // a drag on the rows' labels scrolls them (P8f)
    bool rowsScroll_ = false;  // the rows did not fit last frame
    float wheelRest_ = 0;
    std::optional<TimeView> presetView_;
    std::optional<Time> presetMarker_;
    // The cursor where it was put (--cursor, or a finger's hold and slide, P8f): shown while no
    // pointer is over the waveforms.
    std::optional<Time> placedCursor_;
    bool bus_ = false;                // the pin / bus switch: a click on a pin clips its whole bus
    std::set<std::size_t> selected_;  // Channels picked for a hand group
    std::set<std::string> expanded_;  // group names shown with their members
    std::map<std::vector<std::size_t>, CaptureRuns> runs_;  // for shown_
    std::map<std::string, bool> patternRefused_;  // per group name: its last pattern not taken
};

}  // namespace bw
