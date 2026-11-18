#include "ui/board_keys.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace bw {

namespace {

// The `pc=` name a key event stands for: a printable character (upper case, shifted as typed)
// or one of the named keys; nullopt for any other key.
std::optional<std::string> PcKeyOf(const SDL_KeyboardEvent& key) {
    switch (key.key) {
    case SDLK_SPACE:
        return "Space";
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return "Enter";
    case SDLK_TAB:
        return "Tab";
    case SDLK_BACKSPACE:
        return "Backspace";
    case SDLK_ESCAPE:
        return "Escape";
    case SDLK_UP:
        return "Up";
    case SDLK_DOWN:
        return "Down";
    case SDLK_LEFT:
        return "Left";
    case SDLK_RIGHT:
        return "Right";
    default:
        break;
    }
    const SDL_Keycode code = SDL_GetKeyFromScancode(
        key.scancode, static_cast<SDL_Keymod>(key.mod & SDL_KMOD_SHIFT), false);
    if (code > ' ' && code < 0x7F) {
        return std::string(1, static_cast<char>(std::toupper(static_cast<int>(code))));
    }
    return std::nullopt;
}

}  // namespace

bool BoardKeyCapture::handle(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_UP) {
        held_.erase(event.key.scancode);
        return false;  // passed on, so no key stays held in ImGui
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        held_.clear();  // their key-ups go to another application
        return false;
    }
    if (!on) {
        return false;
    }
    if (event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING) {
        return true;
    }
    if (event.type != SDL_EVENT_KEY_DOWN) {
        return false;
    }
    if (event.key.repeat) {
        return true;
    }
    if (const auto pc = PcKeyOf(event.key)) {
        const auto key = std::ranges::find(keys, *pc, &SwitchKey::pc);
        if (key != keys.end()) {
            held_[event.key.scancode] = key->reference;
        }
    }
    return true;
}

bool BoardKeyCapture::holds(std::string_view reference) const {
    return std::ranges::any_of(held_, [&](const auto& h) { return h.second == reference; });
}

void BoardKeyCapture::sync() {
    if (!on) {
        held_.clear();
    }
}

}  // namespace bw
