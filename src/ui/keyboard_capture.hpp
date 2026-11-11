#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "session/session.hpp"

namespace bw {

// Typing: while on, the keyboard and text events of SDL go to one KEYBOARD Part instead of ImGui,
// so no tool or knob key acts. Escape or F2 ends it.
struct KeyboardCapture {
    bool on = false;
    std::string reference;  // the first of Session::keyboards(); empty: none

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    // Once a frame: text input on while capturing, off when it stops. The ImGui backend stops
    // only text input it started for a widget, so a widget's ending is undone here next frame.
    void sync(SDL_Window* window);

private:
    bool started_ = false;  // text input started by sync()
};

}  // namespace bw
