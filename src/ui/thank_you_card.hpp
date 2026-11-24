#pragma once

#include <cstdint>
#include <string>

namespace bw {

enum class CardAnswer : std::uint8_t { None, Board, Next, Machines };

// The owner's index card thanking the player for a completed Puzzle (P8c): a modal in the main
// window's middle, the bench lightly dimmed and still running behind it. Esc and clicks outside
// do nothing: it waits for a button.
class ThankYouCard {
public:
    void open() { opening_ = true; }  // from the next draw
    // Each frame, last (above every window). `title`: the Puzzle's; `next`: the next Puzzle's
    // title, nullptr for none (no Next button); `canLeave`: Back to the Machines offered. The
    // answer on the frame a button is clicked.
    CardAnswer draw(const std::string& title, const std::string* next, bool canLeave);

private:
    bool opening_ = false;
    bool shown_ = false;  // open: closed by ImGui otherwise is Back to the Board
};

}  // namespace bw
