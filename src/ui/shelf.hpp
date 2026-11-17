#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "runner/runner.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"
#include "view/focus.hpp"

namespace bw {

// The shelf's height along the bottom of the main window, logical px.
inline constexpr float kShelfHeight = 64.0f;

// The strip along the bottom (P6c): controls, the Board's tools, equipment, each a drawing in a
// fixed place on every Machine; what does not apply is grayed. The focus owner is framed in
// gold (red while it cannot work), and the keyboard while it is typed on. A click on equipment
// brings its window out and to the front, or puts it away when already in front. A short hint at
// the right end. Also the keys P, S, M (R), K, W; C, O, V; U.
class Shelf {
public:
    void draw(Session& session, BoardView& board, Focus& focus, Speed& speed, std::uint64_t seed);
    // A click the focus owner could not act on: the reason in amber for 2 s.
    void refused(std::string reason);

    // Equipment that is not an instrument, and the brief: out or away, from the shelf.
    bool tvOut = false, keyboardOut = false, manualOut = false, knobOut = false, briefOut = false;
    bool hasBrief = false;                 // a Puzzle: the brief note is not grayed
    const char* keyboardWindow = nullptr;  // the Machine's keyboard window, by name
    bool keyboardTyping = false;           // its typing on: the keyboard's own focus, in gold
    bool wireStarted = false;              // Wire tool: first pin clicked
    std::string typing;                    // the keyboard typed on; empty: not typing

private:
    struct Devices {
        bool keyboard, knob;
    };
    std::optional<Devices> devices_;
    std::chrono::steady_clock::time_point reasonUntil_{};
    std::string reason_;
};

}  // namespace bw
