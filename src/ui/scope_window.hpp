#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "session/session.hpp"
#include "view/time_view.hpp"

namespace bw {

class BoardView;

// The floating Oscilloscope: settings, the screen with the last Sweep drawn at the current
// settings, Hook rows with values, the status line.
class ScopeWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);
    // From bw_ui, for screenshots: a marker, and a cursor drawn whenever the mouse is not over
    // the screen.
    void preset(std::optional<Time> marker, std::optional<Time> cursor);
    bool focused() const { return focused_; }  // last frame: its Esc is not the Board's

private:
    // "H 3.5 V", "Z 1.4 V", "X", the rail's volts; "" outside the Sweep. `t`: ps from the Trigger.
    std::string valueAt(std::size_t channel, Time t, const std::optional<double>& rail) const;

    std::optional<Sweep> shown_;  // kept across rebuilds and power-off; frozen while stopped
    std::optional<Time> marker_;  // ps from the Trigger
    std::optional<Time> presetCursor_;
    bool focused_ = false;
    bool stopped_ = false;
    float wheelRest_ = 0;
    float panRest_ = 0;                     // px of horizontal wheel not yet a delay step
    std::optional<ScopeSettings> pressed_;  // settings when a drag began
    float pressX_ = 0;
    bool moved_ = false;
    std::array<char, 16> delayText_{};
    bool delayEditing_ = false, delayRefused_ = false;
    // Column cache, per Channel: lo/hi volts per pixel; rebuilt when serial, width, rails or the
    // view change.
    std::array<std::vector<std::pair<float, float>>, kScopeChannels> columns_;
    std::uint64_t columnsSerial_ = 0;
    float columnsWidth_ = 0;
    std::array<std::optional<double>, kScopeChannels> columnsRail_;
    TimeView columnsView_;
};

}  // namespace bw
