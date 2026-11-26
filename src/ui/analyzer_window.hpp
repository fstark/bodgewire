#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "instruments/logic_analyzer.hpp"
#include "session/session.hpp"
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

// The floating Logic Analyzer (P8i): a drawn case, its strip of keys (ARM and the phase's lamps,
// SAMPLE, TRIGGER AT, VIEW / DIV); its screen's rows in the setup's order, each labelled on a
// plate over its left end (swatch, name, value, Trigger key, ×); the shown Capture drawn at the
// view, the cursor's tag on the screen.
class AnalyzerWindow {
public:
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
    // From bw_ui, for screenshots: the view and marker taken at the first Capture; a cursor
    // drawn whenever the mouse is not over the waveforms.
    void preset(std::optional<TimeView> view, std::optional<Time> marker,
                std::optional<Time> cursor);
    // The PIN | BUS switch (P8f) is on BUS: a click on a pin clips or unclips its whole bus, as
    // a Shift-click does.
    bool bus() const { return bus_; }
    // ARM was pressed while paused (P8i): bw_ui resumes the run (Resume), once.
    bool takeRunAsked() { return std::exchange(runAsked_, false); }

private:
    struct Row {
        std::optional<std::size_t> group;  // a group's row, or a member's
        std::size_t channel = 0;           // Channel rows and members
        bool member = false;
        std::size_t place = 0;  // its top row's place (P8i): its colour
    };

    std::vector<Row> rows(const Session& session) const;
    const CaptureRuns& runs(const std::vector<std::size_t>& channels);
    void zoom(Time anchor, int notches);

    std::optional<Capture> shown_;  // kept across rebuilds and power-off
    std::optional<Time> shownPeriod_;
    TimeView view_;
    std::optional<Time> marker_;  // ps from the Trigger
    TimeCanvas canvas_;
    float wheelRest_ = 0;
    float scroll_ = 0;  // the rows' scroll, px (P8i: no child window)
    std::optional<TimeView> presetView_;
    std::optional<Time> presetMarker_;
    // The cursor where it was put (--cursor, or a finger's hold and slide, P8f): shown while no
    // pointer is over the waveforms.
    std::optional<Time> placedCursor_;
    bool bus_ = false;  // PIN | BUS: a click on a pin clips its whole bus
    bool runAsked_ = false;
    std::set<std::size_t> selected_;          // ticked loose Channels, for GROUP (P8i)
    std::optional<std::string> tickedGroup_;  // a ticked group's name, for UNGROUP
    // A plate pressed (P8i, as the scope's): its row's place, where the press began, and whether
    // it has moved past the slop (then it is a drag that moves the row at its release).
    struct Dragging {
        std::size_t place;
        float pressY;
        bool moving;
    };
    std::optional<Dragging> dragging_;
    std::set<std::string> expanded_;                        // group names shown with their members
    std::map<std::vector<std::size_t>, CaptureRuns> runs_;  // for shown_
    std::map<std::string, bool> patternRefused_;  // per group name: its last pattern not taken
};

}  // namespace bw
