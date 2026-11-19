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

// Typing into a key matrix keyboard (the BW-2001's, P5q; the BW-80's, P5x): while on (the Machine
// has the keys and the keyboard is out: P6g), SDL's key and text events go to one keyboard instead
// of ImGui. A PC key types by character: the press that types what the PC key typed, queued in
// simulation time (Session::type: held, then released, so every key is seen however slow the
// Machine runs). On the BW-2001: Return, Backspace, Escape, Home and the arrows type RETURN, DEL,
// STOP, HOME and CRSR (left and up with SHIFT). On the BW-80: Return NEW LINE, Shift+Return EDIT,
// Backspace RUBOUT, Escape BREAK, Home HOME, the arrows the cursor keys; Shift with a letter is
// SHIFT with that key (the graphics, the symbols, OR and **: the ROM gives the meaning). Cmd+V
// or Ctrl+V pastes the clipboard as presses (the keyboard's own map: a BASIC listing keyword by
// keyword on the BW-80). F2 is main.cpp's.
struct MatrixCapture {
    bool on = false;
    std::string reference;  // the first of Session::matrixKeyboards(); empty: none
    KeyMatrixKind kind = KeyMatrixKind::Bw2001;  // Session::matrixKind(reference): the key map

    // The press that types `c` on this keyboard (MatrixKeyFor or Zx80KeyFor).
    std::optional<MatrixPress> pressFor(char32_t c) const;

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    void sync(SDL_Window* focused);  // as KeyboardCapture::sync

    // The presses typed by the PC keys held now (drawn down).
    std::vector<MatrixPress> held() const;

private:
    void send(Session& session, SDL_Scancode scancode, char32_t c);

    SDL_Window* started_ = nullptr;
    std::optional<SDL_Scancode> lastDown_;  // the key whose text event is due
    bool dropText_ = false;                 // the text event due was typed as a press already
    std::map<SDL_Scancode, MatrixPress> held_;
};

}  // namespace bw
