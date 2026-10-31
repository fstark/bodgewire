#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "runner/runner.hpp"
#include "session/session.hpp"
#include "ui/board_view.hpp"

namespace bw {

// A large toggle button; true when clicked.
bool ToolButton(const char* label, bool selected);

// State banner and the row of buttons above the Board panel.
class Toolbar {
public:
    void draw(Session& session, BoardView& board, Speed& speed, std::uint64_t seed);
    // A click the tool could not act on: flash amber; a reason replaces the action text for 2 s.
    void refused(std::string reason = {});

    bool userManualOpen = false, serviceManualOpen = false;
    bool analyzerOpen = false, scopeOpen = false, meterOpen = false;
    bool wireStarted = false;  // Wire tool: first pin clicked

private:
    std::chrono::steady_clock::time_point flashUntil_{}, reasonUntil_{};
    std::string reason_;
};

}  // namespace bw
