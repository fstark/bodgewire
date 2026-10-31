#pragma once

#include "session/session.hpp"

namespace bw {

class BoardView;

// The floating Multimeter: tool button, mode, large readout, what each Test Lead is on.
class MeterWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);
};

}  // namespace bw
