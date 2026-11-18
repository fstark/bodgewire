#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "session/session.hpp"

namespace bw {

// The knob window's name, for the shelf to bring it to the front.
inline constexpr const char* kKnobWindowName = "Knob";

// The PC keys held down for the knobs (P6g): from SDL's events, while the Machine has the keys
// and the knob box is out. Forgotten when the window loses the system's focus (their key-ups go
// to another application).
class KnobKeys {
public:
    void handle(const SDL_Event& event, bool on);  // every event, before ImGui
    bool down(SDL_Scancode key) const { return held_.contains(key); }
    void clear() { held_.clear(); }

private:
    std::set<SDL_Scancode> held_;
};

// The paddle controllers as an object on the bench (P6d): one dark box a controller, side by
// side, each a large knob turned by a vertical drag or the mouse wheel. No title bar: dragged by
// the boxes, resized from the corner keeping their proportions, put away by the × on the case.
class KnobWindow {
public:
    // Up and Down turn the knob (Q and A the first of two) while held and `on` (the Machine has
    // the keys and the box is out: P6g). Every frame.
    void keys(Session& session, const KnobKeys& held, bool on);
    // `gold`: the Machine has the keys. True when the box was clicked (not its ×).
    bool draw(bool& open, Session& session, bool gold);

private:
    const std::vector<std::string>& knobs(const Session& session);

    std::optional<std::vector<std::string>> knobs_;  // found once: knobs() scans the parts
};

}  // namespace bw
