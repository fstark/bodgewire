#pragma once

#include <chrono>
#include <cstdint>

#include "runner/runner.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"

namespace bw {

// State banner and the row of buttons above the Board panel.
class Toolbar {
public:
    void draw(Session& session, BoardView& board, Speed& speed, std::uint64_t seed);
    void refused();  // a click the tool could not act on: flash amber

    bool userManualOpen = false, serviceManualOpen = false, analyzerOpen = false;

private:
    std::chrono::steady_clock::time_point flashUntil_{};
};

}  // namespace bw
