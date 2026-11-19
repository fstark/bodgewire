#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "runner/runner.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"
#include "ui/shelf_drawings.hpp"
#include "view/focus.hpp"

namespace bw {

// The shelf's height along the bottom of the main window, logical px.
inline constexpr float kShelfHeight = 64.0f;

// Power on with the run switch (P6o): at `‖` it jumps to `1×` first (powering on always runs).
void PowerOn(Session& session, RunSwitch& run, std::uint64_t seed);

// The strip along the bottom (P6c, regrouped in P6o): each group in its own tray, the controls,
// the Board's tools, the instruments from the left; this Machine's brief (only on a Puzzle),
// binder and Machine slot (its TV, keyboard and knob box) at the right end; a short hint between.
// What does not apply is grayed. The focus owner is framed in gold. A click on equipment brings it
// out and to the front, or puts it away when already in front; a double-click pops it out to its
// own desktop window, or back in.
class Shelf {
public:
    void draw(Session& session, BoardView& board, Focus& focus, RunSwitch& run, std::uint64_t seed);
    // A click the focus owner could not act on: the reason in amber for 2 s.
    void refused(std::string reason);

    // Equipment that is not an instrument, and the brief: out or away, from the shelf.
    bool tvOut = false, keyboardOut = false, manualOut = false, knobOut = false, briefOut = false;
    bool hasBrief = false;                 // a Puzzle: the brief note is on the shelf
    const char* keyboardWindow = nullptr;  // the Machine's keyboard window, by name
    bool wireStarted = false;              // Wire tool: first pin clicked

private:
    struct Devices {
        bool tv, keyboard, knob;
    };
    void machineClick(bool second);

    Devices devices_{};
    bool found_ = false;  // devices_ found (a Machine's devices never change)
    std::chrono::steady_clock::time_point reasonUntil_{};
    std::string reason_;
};

}  // namespace bw
