#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "session/session.hpp"
#include "ui/gestures.hpp"
#include "view/time_view.hpp"

namespace bw {

// The floating Oscilloscope: settings, the screen with the last Sweep drawn at the current
// settings, Hook rows with values, the status line.
class ScopeWindow {
public:
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
    // From bw_ui, for screenshots: a marker, and a cursor drawn whenever the mouse is not over
    // the screen.
    void preset(std::optional<Time> marker, std::optional<Time> cursor);

private:
    // "H 3.5 V", "Z 1.4 V", "X", the rail's volts; "" outside the Sweep. `t`: ps from the Trigger.
    std::string valueAt(std::size_t channel, Time t, const std::optional<double>& rail) const;

    std::optional<Sweep> shown_;  // kept across rebuilds and power-off; frozen while stopped
    std::optional<Time> marker_;  // ps from the Trigger
    // The cursor where it was put (--cursor, or a finger's hold and slide, P8f).
    std::optional<Time> placedCursor_;
    bool stopped_ = false;
    TimeCanvas canvas_;
    float panRest_ = 0;                     // px of horizontal wheel not yet a delay step
    std::optional<ScopeSettings> pressed_;  // settings when a drag began
    bool delayRefused_ = false;             // the position last entered was not taken
    // Column cache, per Channel: lo/hi volts per pixel; rebuilt when serial, width, rails or the
    // view change.
    std::array<std::vector<std::pair<float, float>>, kScopeChannels> columns_;
    std::uint64_t columnsSerial_ = 0;
    float columnsWidth_ = 0;
    std::array<std::optional<double>, kScopeChannels> columnsRail_;
    TimeView columnsView_;
};

}  // namespace bw
