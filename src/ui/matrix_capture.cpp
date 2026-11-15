#include "ui/matrix_capture.hpp"

namespace bw {

void MatrixCapture::send(Session& session, SDL_Scancode scancode, char32_t c) {
    session.type(reference, c);  // refused with the power off: the caps still move
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }
    if (const auto press = MatrixKeyFor(c)) {
        held_[scancode] = *press;
    }
}

bool MatrixCapture::handle(const SDL_Event& event, Session& session) {
    if (event.type == SDL_EVENT_KEY_UP) {
        held_.erase(event.key.scancode);
        return false;  // passed on, so no key stays held in ImGui
    }
    if (!on) {
        return false;
    }
    switch (event.type) {
    case SDL_EVENT_TEXT_INPUT:
        for (const char* c = event.text.text; *c != '\0'; ++c) {
            const auto byte = static_cast<unsigned char>(*c);
            if (byte >= 0x20 && byte < 0x7F) {
                send(session, lastDown_.value_or(SDL_SCANCODE_UNKNOWN), byte);
            }
        }
        lastDown_.reset();
        return true;
    case SDL_EVENT_TEXT_EDITING:
        return true;
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Scancode scancode = event.key.scancode;
        const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        if ((event.key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)) != 0 && event.key.key == SDLK_V) {
            if (char* text = SDL_GetClipboardText()) {
                for (const char* c = text; *c != '\0'; ++c) {
                    session.type(reference, static_cast<unsigned char>(*c));
                }
                SDL_free(text);
            }
            return true;
        }
        switch (event.key.key) {
        case SDLK_F2:
            stop = true;
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            send(session, scancode, '\r');
            return true;
        case SDLK_BACKSPACE:
            send(session, scancode, shift ? kKeyInst : kKeyDel);
            return true;
        case SDLK_ESCAPE:
            send(session, scancode, kKeyStop);
            return true;
        case SDLK_HOME:
            send(session, scancode, shift ? kKeyClr : kKeyHome);
            return true;
        case SDLK_UP:
            send(session, scancode, kKeyUp);
            return true;
        case SDLK_DOWN:
            send(session, scancode, kKeyDown);
            return true;
        case SDLK_LEFT:
            send(session, scancode, kKeyLeft);
            return true;
        case SDLK_RIGHT:
            send(session, scancode, kKeyRight);
            return true;
        default:
            break;
        }
        lastDown_ = scancode;
        return true;
    }
    default:
        return false;
    }
}

void MatrixCapture::sync(SDL_Window* window) {
    if (on && !SDL_TextInputActive(window)) {
        SDL_StartTextInput(window);
        started_ = true;
    } else if (!on && started_) {
        SDL_StopTextInput(window);
        started_ = false;
    }
    if (!on) {
        held_.clear();
        lastDown_.reset();
    }
}

std::vector<MatrixPress> MatrixCapture::held() const {
    std::vector<MatrixPress> presses;
    for (const auto& [scancode, press] : held_) {
        presses.push_back(press);
    }
    return presses;
}

}  // namespace bw
