#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <imgui.h>

#include "session/session.hpp"
#include "ui/keyboard_capture.hpp"
#include "view/keyboard_layout.hpp"

namespace bw {

// Roboto Medium (Apache License 2.0, from Dear ImGui's misc/fonts), embedded at build:
// keycap_font.cpp.in.
extern const unsigned char kKeycapFont[];
extern const std::size_t kKeycapFontSize;

// The keyboard window's name, for the shelf to bring it to the front.
inline constexpr const char* kKeyboardWindowName = "Keyboard";

// The Machine's ASCII keyboard as a floating window of drawn caps (the first KEYBOARD). Typing
// while it is focused (KeyboardCapture), F2 toggles; clicks type caps; --type keys flash.
class KeyboardWindow {
public:
    explicit KeyboardWindow(ImFont* legends) : legends_(legends) {}

    // `capture.on` follows the window's focus. Nothing drawn without a keyboard.
    void draw(bool& open, Session& session, KeyboardCapture& capture);
    // A code sent other than by this window (--type): its cap and modifiers down 150 ms, or
    // until the next flash.
    void flash(std::uint8_t code);

private:
    void drawCaps(Session& session, const KeyboardCapture& capture);

    ImFont* legends_;
    bool shiftLatched_ = false, ctrlLatched_ = false;  // mouse: until the next cap is released
    std::optional<CapPress> flash_;
    std::chrono::steady_clock::time_point flashUntil_{};
    bool focusNext_ = false;  // F2 while not typing
};

}  // namespace bw
