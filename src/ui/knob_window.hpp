#pragma once

#include <optional>
#include <string>
#include <vector>

#include "session/session.hpp"

namespace bw {

// The knob window's name, for the shelf to bring it to the front.
inline constexpr const char* kKnobWindowName = "Knob";

// The paddle controllers as an object on the bench (P6d): one dark box a controller, side by
// side, each a large knob turned by a vertical drag or the mouse wheel. No title bar: dragged by
// the boxes, resized from the corner keeping their proportions, put away by the × on the case.
class KnobWindow {
public:
    // Up and Down turn the knob (Q and A the first of two) while held, out or away, unless text
    // input has the keyboard. Every frame.
    void keys(Session& session);
    void draw(bool& open, Session& session);

private:
    const std::vector<std::string>& knobs(const Session& session);

    std::optional<std::vector<std::string>> knobs_;  // found once: knobs() scans the parts
};

}  // namespace bw
