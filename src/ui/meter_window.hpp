#pragma once

#include "session/session.hpp"

namespace bw {

// The floating Multimeter: mode, large readout, what each Test Lead is on.
class MeterWindow {
public:
    // True when clicked: it takes the focus.
    bool draw(bool& open, Session& session);
};

}  // namespace bw
