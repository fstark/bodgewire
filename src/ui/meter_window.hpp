#pragma once

#include "session/session.hpp"

namespace bw {

// The floating Multimeter: mode, large readout, what each Test Lead is on.
class MeterWindow {
public:
    // `owner`: it has the focus (a gold frame). True when clicked: it takes the focus.
    bool draw(bool& open, bool owner, Session& session);
};

}  // namespace bw
