#include "ui/keyboard_capture.hpp"

namespace bw {

bool KeyboardCapture::handle(const SDL_Event& event, Session& session) {
    if (!on) {
        return false;
    }
    switch (event.type) {
    case SDL_EVENT_TEXT_INPUT:
        // Printable ASCII only: Return and Backspace come from KEY_DOWN, so a platform that also
        // sends them as text does not type twice; non-ASCII bytes are not keys of the keyboard.
        for (const char* c = event.text.text; *c != '\0'; ++c) {
            const auto byte = static_cast<unsigned char>(*c);
            if (byte >= 0x20 && byte < 0x7F) {
                session.type(reference, byte);
            }
        }
        return true;
    case SDL_EVENT_TEXT_EDITING:
        return true;
    case SDL_EVENT_KEY_DOWN:
        switch (event.key.key) {
        case SDLK_ESCAPE:
        case SDLK_F2:
            on = false;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            session.type(reference, U'\r');
            break;
        case SDLK_BACKSPACE:
            session.type(reference, U'\b');
            break;
        default:
            break;
        }
        return true;
    default:
        return false;  // key up included, so no key stays held in ImGui
    }
}

void KeyboardCapture::sync(SDL_Window* window) {
    if (on && !SDL_TextInputActive(window)) {
        SDL_StartTextInput(window);
        started_ = true;
    } else if (!on && started_) {
        SDL_StopTextInput(window);
        started_ = false;
    }
}

}  // namespace bw
