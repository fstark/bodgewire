#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "session/session.hpp"
#include "view/keyboard_layout.hpp"

namespace bw {

// Typing: while on (the keyboard window focused), the keyboard and text events of SDL go to one
// KEYBOARD Part instead of ImGui, so no tool or knob key acts. F2 asks to stop (`stop`); Escape
// is a key of the keyboard.
struct KeyboardCapture {
    bool on = false;
    bool stop = false;      // F2 pressed while typing; the keyboard window clears it
    std::string reference;  // the first of Session::keyboards(); empty: none

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    // Once a frame: text input on while capturing, off when it stops (and the keys held
    // forgotten: their key-ups may go elsewhere). The ImGui backend stops only text input it
    // started for a widget, so a widget's ending is undone here next frame.
    void sync(SDL_Window* window);

    // The caps typed by the PC keys held now, each with the modifiers it needed.
    std::vector<CapPress> held() const;
    // PC Shift and Ctrl held while typing (SDL_GetModState: also pressed before typing began).
    bool shiftHeld() const;
    bool ctrlHeld() const;

private:
    void send(Session& session, SDL_Scancode scancode, std::uint8_t code);

    bool started_ = false;                  // text input started by sync()
    std::optional<SDL_Scancode> lastDown_;  // the key whose text event is due
    std::map<SDL_Scancode, CapPress> held_;
};

}  // namespace bw
