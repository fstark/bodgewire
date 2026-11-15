#pragma once

#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "parts/matrix_keyboard.hpp"
#include "session/session.hpp"

namespace bw {

// Typing into the BW-2001 keyboard (P5q): while on (its window focused), SDL's key and text events
// go to one MATRIXKB instead of ImGui. A PC key types by character: the press that types what the
// PC key typed, queued in simulation time (Session::type: 40 ms down, 40 ms up, so every key is
// seen however slow the Machine runs); Return, Backspace, Escape, Home and the arrows type RETURN,
// DEL, STOP, HOME and CRSR (left and up with SHIFT). Cmd+V or Ctrl+V pastes the clipboard as
// presses. F2 asks to stop (`stop`).
struct MatrixCapture {
    bool on = false;
    bool stop = false;      // F2 pressed while typing; the window clears it
    std::string reference;  // the first of Session::matrixKeyboards(); empty: none

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    void sync(SDL_Window* window);  // as KeyboardCapture::sync

    // The presses typed by the PC keys held now (drawn down).
    std::vector<MatrixPress> held() const;

private:
    void send(Session& session, SDL_Scancode scancode, char32_t c);

    bool started_ = false;
    std::optional<SDL_Scancode> lastDown_;  // the key whose text event is due
    std::map<SDL_Scancode, MatrixPress> held_;
};

}  // namespace bw
