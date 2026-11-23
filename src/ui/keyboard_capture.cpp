#include "ui/keyboard_capture.hpp"

#include "parts/keyboard.hpp"

namespace bw {

namespace {

constexpr std::uint8_t kReturn = 0x0D, kBackSpace = 0x08, kEscape = 0x1B;

}  // namespace

void KeyboardCapture::send(Session& session, SDL_Scancode scancode, std::uint8_t code) {
    session.inputs().typeCode(reference, code);  // refused with the power off: the caps still move
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }
    if (const auto press = CapFor(code)) {
        held_[scancode] = *press;
    }
}

bool KeyboardCapture::handle(const SDL_Event& event, Session& session) {
    if (event.type == SDL_EVENT_KEY_UP) {
        held_.erase(event.key.scancode);
        return false;  // passed on, so no key stays held in ImGui
    }
    if (!on) {
        return false;
    }
    switch (event.type) {
    case SDL_EVENT_TEXT_INPUT:
        // Printable ASCII only: Return, Backspace, Escape and Ctrl codes come from KEY_DOWN, so a
        // platform that also sends them as text does not type twice.
        for (const char* c = event.text.text; *c != '\0'; ++c) {
            const auto byte = static_cast<unsigned char>(*c);
            if (byte < 0x20 || byte >= 0x7F) {
                continue;
            }
            if (const auto code = KeyboardCode(byte)) {
                send(session, lastDown_.value_or(SDL_SCANCODE_UNKNOWN), *code);
            }
        }
        lastDown_.reset();
        return true;
    case SDL_EVENT_TEXT_EDITING:
        return true;
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Scancode scancode = event.key.scancode;
        switch (event.key.key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            send(session, scancode, kReturn);
            return true;
        case SDLK_BACKSPACE:
            send(session, scancode, kBackSpace);
            return true;
        case SDLK_ESCAPE:
            send(session, scancode, kEscape);
            return true;
        default:
            break;
        }
        if ((event.key.mod & SDL_KMOD_CTRL) != 0) {
            // event.key.key is the unshifted key: Ctrl+Shift+2 must give '@' on a US layout.
            SDL_Keycode key = SDL_GetKeyFromScancode(
                scancode, static_cast<SDL_Keymod>(event.key.mod & SDL_KMOD_SHIFT), false);
            if (key >= 'a' && key <= 'z') {
                key -= 0x20;
            }
            if (key >= 0x40 && key <= kKeyboardLastCode) {
                send(session, scancode, static_cast<std::uint8_t>(key & 0x1F));
            } else if (key >= 0x20 && key < 0x40) {
                send(session, scancode, static_cast<std::uint8_t>(key));  // CTRL changes nothing
            }
            return true;
        }
        lastDown_ = scancode;
        return true;
    }
    default:
        return false;
    }
}

void KeyboardCapture::sync(SDL_Window* focused) {
    if (started_ != nullptr && (!on || started_ != focused)) {
        SDL_StopTextInput(started_);
        started_ = nullptr;
    }
    if (on && !SDL_TextInputActive(focused)) {
        SDL_StartTextInput(focused);
        started_ = focused;
    }
    if (!on) {
        held_.clear();
        lastDown_.reset();
    }
}

std::vector<CapPress> KeyboardCapture::held() const {
    std::vector<CapPress> presses;
    for (const auto& [scancode, press] : held_) {
        presses.push_back(press);
    }
    return presses;
}

bool KeyboardCapture::shiftHeld() const { return on && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0; }

bool KeyboardCapture::ctrlHeld() const { return on && (SDL_GetModState() & SDL_KMOD_CTRL) != 0; }

}  // namespace bw
