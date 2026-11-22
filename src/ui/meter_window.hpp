#pragma once

#include <utility>

#include "session/session.hpp"

namespace bw {

// The floating Multimeter: mode, large readout, what each Test Lead is on. In Continuity, the
// Shelf's power switch beside the readout, to power the Machine off (and on) from here.
class MeterWindow {
public:
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
    // The power switch clicked since the last call: the caller powers off or on, as the Shelf's.
    bool takePowerClick() { return std::exchange(powerClicked_, false); }

private:
    bool powerClicked_ = false;
};

}  // namespace bw
