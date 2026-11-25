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
#include "view/pace.hpp"

namespace bw {

// The shelf's wood and its trays' shadow, also the launcher's post-its' lane (P7b).
inline constexpr ImU32 kShelfWood = IM_COL32(48, 38, 30, 255);
inline constexpr ImU32 kShelfEdge = IM_COL32(92, 74, 58, 255);
inline constexpr ImU32 kTrayShadow = IM_COL32(28, 22, 17, 255);

// The shelf's height along the bottom of the main window, logical px.
inline constexpr float kShelfHeight = 64.0f;

// Power on with the run switch (P6o): at `‖` it jumps to `1×` first (powering on always runs).
void PowerOn(Session& session, RunSwitch& run, std::uint64_t seed);
// The Runner's speed at a run switch position other than `‖`.
Speed SpeedOf(RunSwitch run);

// The strip along the bottom (P6c, regrouped in P6o): each group in its own tray, the way back to
// the launcher (P7a), the controls, the Board's tools, the instruments from the left; this
// Machine's brief (only on a Puzzle), binder and Machine slot (its TV, keyboard and knob box) at
// the right end; a short hint between. What does not apply is grayed. The focus owner is framed in
// gold. A click on equipment brings it out and to the front, or puts it away when already in front;
// a double-click pops it out to its own desktop window, or back in.
class Shelf {
public:
    void draw(Session& session, BoardView& board, Focus& focus, RunSwitch& run, std::uint64_t seed);
    // A click the focus owner could not act on: the reason in amber for 2 s.
    void refused(std::string reason);

    bool hasBrief = false;                 // a Puzzle: the brief note is on the shelf
    const char* keyboardWindow = nullptr;  // the Machine's keyboard window, by name
    bool wireStarted = false;              // Wire tool: first pin clicked
    bool canLeave = false;      // a machines/ known: the Leave tray's arrow is live (P7a)
    bool leaveClicked = false;  // set by draw() on a click on it, reset by its caller
    ImVec2 leaveAt{};  // above the Leave arrow, screen px: where its question opens upwards
    bool resetClicked = false;  // the Reset (every repair undone), as leaveClicked
    ImVec2 resetAt{};           // above it, as leaveAt
    ImVec2 meterAt{};  // the Multimeter's drawing's centre x on the shelf's top edge (P8g)
    bool running = false;       // the runner running (not off, not paused)
    double speedRatio = 0;      // simulated over wall time, shown on the run switch

private:
    struct Devices {
        bool tv, keyboard, knob;
    };
    void machineClick(Focus& focus, bool second);

    Devices devices_{};
    bool found_ = false;  // devices_ found (a Machine's devices never change)
    PaceWatch pace_;
    std::chrono::steady_clock::time_point reasonUntil_{};
    std::string reason_;
};

}  // namespace bw
