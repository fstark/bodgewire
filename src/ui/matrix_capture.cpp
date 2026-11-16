#include "ui/matrix_capture.hpp"

#include <string>
#include <string_view>

namespace bw {

namespace {

// The clipboard's UTF-8 as code points; a malformed byte becomes U+FFFD.
std::u32string DecodeUtf8(std::string_view text) {
    std::u32string out;
    for (std::size_t i = 0; i < text.size();) {
        const auto b = static_cast<unsigned char>(text[i]);
        std::size_t n = b < 0x80             ? 1
                        : (b & 0xE0) == 0xC0 ? 2
                        : (b & 0xF0) == 0xE0 ? 3
                        : (b & 0xF8) == 0xF0 ? 4
                                             : 0;
        if (n == 0 || i + n > text.size()) {
            out.push_back(U'\uFFFD');
            ++i;
            continue;
        }
        char32_t c = n == 1 ? b : b & (0x7F >> n);
        for (std::size_t k = 1; k < n; ++k) {
            c = (c << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }
        out.push_back(c);
        i += n;
    }
    return out;
}

}  // namespace

std::optional<MatrixPress> MatrixCapture::pressFor(char32_t c) const {
    return kind == KeyMatrixKind::Bw80 ? Zx80KeyFor(c) : MatrixKeyFor(c);
}

void MatrixCapture::send(Session& session, SDL_Scancode scancode, char32_t c) {
    session.type(reference, c);  // refused with the power off: the caps still move
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }
    if (const auto press = pressFor(c)) {
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
                session.typeText(reference, DecodeUtf8(text));  // the keyboard's own map
                SDL_free(text);
            }
            return true;
        }
        const bool bw80 = kind == KeyMatrixKind::Bw80;
        switch (event.key.key) {
        case SDLK_F2:
            stop = true;
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            send(session, scancode, bw80 && shift ? kKeyEdit : U'\r');
            return true;
        case SDLK_BACKSPACE:
            send(session, scancode, shift && !bw80 ? kKeyInst : kKeyDel);
            return true;
        case SDLK_ESCAPE:
            send(session, scancode, bw80 ? kKeyBreak : kKeyStop);
            return true;
        case SDLK_HOME:
            send(session, scancode, shift && !bw80 ? kKeyClr : kKeyHome);
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
