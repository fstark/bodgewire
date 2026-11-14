#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "session/session.hpp"

namespace bw {

// Typing on the Board's keys (P5i): while on, a PC key held down holds the switch whose `pc=` it
// is, several at once; every other key down is swallowed so no tool or knob key acts. F2 turns
// it off. Only on a Machine with switch PC keys and no KEYBOARD (that one types instead).
struct BoardKeyCapture {
    bool on = false;
    std::vector<SwitchKey> keys;  // Session::switchKeys(); empty: no Board keys

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event);
    bool holds(std::string_view reference) const;
    void sync();  // once a frame: nothing held while off

private:
    std::map<SDL_Scancode, std::string> held_;  // scancode → switch reference
};

}  // namespace bw
