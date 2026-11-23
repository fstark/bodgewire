#pragma once

#include <cstddef>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "machine/press.hpp"
#include "machine/typing.hpp"
#include "session/session.hpp"
#include "ui/board_keys.hpp"
#include "ui/board_view.hpp"
#include "ui/keyboard_window.hpp"

namespace bw {

// The Board's switches as the player's hands and the flags hold them (P5i, P6o): a button held
// under the mouse or by its PC key, kept pressed by a right-click (released at power-off), a SLIDE
// left where it was moved (powered or not), or pressed by --press. Also without a Board.
class SwitchHands {
public:
    // Once a frame: every switch's state to the Session, their tags onto `view`.
    void update(Session& session, BoardView& view, std::span<const ButtonPress> presses,
                const BoardKeyCapture& keys, Time simTime);

private:
    std::set<std::string> latched_;  // buttons kept pressed by a right-click
    std::set<std::string> slid_;     // SLIDE switches at ON
};

// --type's keys sent in simulation time (P5k): a KEYBOARD's one by one at their times, a key
// matrix's text queued whole at its start (the matrix paces its own presses). Again after each
// power-on.
class ScriptedTyping {
public:
    explicit ScriptedTyping(std::span<const Typing> typing)
        : typing_(typing.begin(), typing.end()), sent_(typing.size(), 0) {}
    // Once a frame. `matrix`: the key matrix's reference ("" none); `keyboard`'s window flashes
    // the keys typed on it.
    void update(Session& session, Time simTime, std::string_view matrix, std::string_view keyboard,
                KeyboardWindow& window);

private:
    std::vector<Typing> typing_;
    std::vector<std::size_t> sent_;  // keys sent so far, per Typing
    bool wasPowered_ = false;
};

}  // namespace bw
