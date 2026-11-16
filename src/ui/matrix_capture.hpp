#pragma once

#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "parts/key_matrix.hpp"
#include "parts/matrix_keyboard.hpp"
#include "parts/zx80_keys.hpp"
#include "session/session.hpp"

namespace bw {

// Typing into a key matrix keyboard (the BW-2001's, P5q; the BW-80's, P5x): while on (its window
// focused), SDL's key and text events go to one keyboard instead of ImGui. A PC key types by
// character: the press that types what the PC key typed, queued in simulation time (Session::type:
// held, then released, so every key is seen however slow the Machine runs). On the BW-2001:
// Return, Backspace, Escape, Home and the arrows type RETURN, DEL, STOP, HOME and CRSR (left and
// up with SHIFT). On the BW-80: Return NEW LINE, Shift+Return EDIT, Backspace RUBOUT, Escape
// BREAK, Home HOME, the arrows the cursor keys. Cmd+V or Ctrl+V pastes the clipboard as presses
// (the keyboard's own map: a BASIC listing keyword by keyword on the BW-80). F2 asks to stop
// (`stop`).
struct MatrixCapture {
    bool on = false;
    bool stop = false;      // F2 pressed while typing; the window clears it
    std::string reference;  // the first of Session::matrixKeyboards(); empty: none
    KeyMatrixKind kind = KeyMatrixKind::Bw2001;  // Session::matrixKind(reference): the key map

    // The press that types `c` on this keyboard (MatrixKeyFor or Zx80KeyFor).
    std::optional<MatrixPress> pressFor(char32_t c) const;

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
