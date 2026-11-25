#pragma once

#include <cstddef>
#include <span>
#include <string>

#include <imgui.h>

#include "view/number_pad.hpp"

namespace bw {

// The controls every window shares (P8f): one size for the mouse and the finger, and what
// replaces a dropdown or a text field, which a finger cannot work.

inline constexpr float kControl = 28;  // a control's height: the style's frame
inline constexpr float kBig = 44;      // a new control's, a popup button's

// Each ImGui context, once: frames kControl high, wider scrollbars.
void ApplyStyle();

// A text on a line of kBig controls (beside a Stepper), centred on its height; `colour` 0: the
// text's own.
void BigText(const char* text, ImU32 colour = 0);

// -, the value, +: the two buttons kBig square. + is the next value, - the one before: give the
// values from coarse to fine, so + is always the finer step. A click on the value opens every
// value in two columns of kBig lines. True when `index` changed.
bool Stepper(const char* id, std::span<const std::string> values, std::size_t& index,
             float valueWidth);

// Buttons side by side, the `index`-th lit: for 2 to 5 choices. True when it changed.
bool Choice(const char* id, std::span<const char* const> labels, std::size_t& index,
            float height = kControl);

// The value as one button `width` wide; a click opens the choices as a strip of kBig buttons.
// True when it changed.
bool ChoicePopup(const char* id, std::span<const char* const> labels, std::size_t& index,
                 float width, float height = kControl);

// In a scrolling child window, after its content: a drag on it scrolls it, with a short glide
// after the lift (P8f; the wheel still scrolls), and so do two fingers. The press of the item the
// drag began on is let go: a scroll is no click. `taken`: something else has the drag (a time
// screen): nothing.
class DragScroll {
public:
    void update(bool taken = false);
    // The window was scrolled by a drag or two fingers this frame.
    bool scrolled() const { return scrolled_; }

private:
    bool pressed_ = false;   // the left button went down in the window, off its scrollbar
    bool dragging_ = false;  // ... and moved past the drag threshold
    bool scrolled_ = false;
    float velocity_ = 0;  // px a second, the glide's
};

// A number's field: its text in a frame `width` wide. A click opens the number pad beside it,
// never under the pointer, showing the value; the pad's keys and the PC's (the digits, A to F, X,
// the point, N, U, M then S for a unit, Backspace, Enter, Escape) edit a copy of at most `limit`
// characters. True once, when something typed is confirmed: `text` then holds it. `refused`:
// the field drawn red (the last value was not taken).
bool NumberField(const char* id, std::string& text, PadKind kind, float width, bool refused = false,
                 std::size_t limit = 16);
// A pad is open: the PC's keys are its own this frame, not the Machine's.
bool NumberPadOpen();

}  // namespace bw
