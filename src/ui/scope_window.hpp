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

// The floating Oscilloscope (P8h): its screen in bands, one per Hook in the scope's order, each
// labelled on a plate over its left end (pin, reading, Trigger, ×); the last Sweep drawn at the
// current settings.
class ScopeWindow {
public:
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
    // From bw_ui, for screenshots: a marker, and a cursor drawn whenever the mouse is not over
    // the screen.
    void preset(std::optional<Time> marker, std::optional<Time> cursor);

private:
    std::optional<Sweep> shown_;  // kept across rebuilds and power-off
    std::optional<Time> marker_;  // ps from the Trigger
    // The cursor where it was put (--cursor, or a finger's hold and slide, P8f).
    std::optional<Time> placedCursor_;
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
    // A band's reading, per Channel, and what it was made from (P8h).
    struct Reading {
        std::string text;
        std::uint64_t serial = 0;
        Time from = 0, to = 0;
        bool powered = false;
        std::optional<double> rail;
    };
    std::array<Reading, kScopeChannels> readings_;
    // A plate pressed (P8h): its band's place, where the press began, and whether it has moved
    // past the slop (then it is a drag that moves the band at its release).
    struct Dragging {
        std::size_t place;
        float pressY;
        bool moving;
    };
    std::optional<Dragging> dragging_;
};

}  // namespace bw
