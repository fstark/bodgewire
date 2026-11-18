#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "session/session.hpp"
#include "view/keyboard_layout.hpp"

namespace bw {

// Typing: while on (the Machine has the keys and the keyboard is out: P6g), the keyboard and text
// events of SDL go to one KEYBOARD Part instead of ImGui, so no tool or knob key acts. Escape is
// a key of the keyboard; F2 is main.cpp's (it gives the keys back).
struct KeyboardCapture {
    bool on = false;
    std::string reference;  // the first of Session::keyboards(); empty: none

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    // Once a frame: text input on while capturing, off when it stops (and the keys held
    // forgotten: their key-ups may go elsewhere). The ImGui backend stops only text input it
    // started for a widget, so a widget's ending is undone here next frame. SDL3's text input is
    // per window: on `focused`, the window with the keyboard focus (a popped-out keyboard's,
    // P6j), and moved off the one it was on when the focus moves.
    void sync(SDL_Window* focused);

    // The caps typed by the PC keys held now, each with the modifiers it needed.
    std::vector<CapPress> held() const;
    // PC Shift and Ctrl held while typing (SDL_GetModState: also pressed before typing began).
    bool shiftHeld() const;
    bool ctrlHeld() const;

private:
    void send(Session& session, SDL_Scancode scancode, std::uint8_t code);

    SDL_Window* started_ = nullptr;         // text input started by sync(), on this window
    std::optional<SDL_Scancode> lastDown_;  // the key whose text event is due
    std::map<SDL_Scancode, CapPress> held_;
};

}  // namespace bw
