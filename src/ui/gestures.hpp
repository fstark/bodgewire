#pragma once

#include <optional>

namespace bw {

// What the player did this frame on a time canvas (the Oscilloscope's screen, the Logic
// Analyzer's lanes), read from the item just submitted (an InvisibleButton): the gestures both
// windows share, so another input (touch, docs/p8f-touch/TOUCH.md) lands in one place. Positions
// are screen px.
struct TimeCanvasInput {
    std::optional<float> hover;  // under the pointer; nullopt off the canvas
    int zoom = 0;                // wheel notches at `hover`: positive zooms in
    float pan = 0;               // horizontal wheel, px of the canvas (half a division a notch)
    bool fit = false;            // double-click
    bool clearMarker = false;    // right-click
    bool pressed = false;        // a press began this frame
    float dragStep = 0;          // px moved this frame while pressed
    std::optional<float> dragFromPress;  // px since the press, once past a click's slop
    std::optional<float> marker;         // a click without a drag, released here
    bool released = false;               // the press ended this frame
};

class TimeCanvas {
public:
    // After the canvas's InvisibleButton: `width` px wide, `divisions` across.
    TimeCanvasInput read(float width, int divisions);

private:
    float wheelRest_ = 0;
    float pressX_ = 0;
    bool moved_ = false;
};

// A round knob's input this frame, read from the item just submitted (an InvisibleButton over
// it): a vertical drag (up turns it up) and the mouse wheel, as the TV's controls and the paddle
// controllers take them. Turns: 1 is the whole sweep.
struct KnobInput {
    bool hovered = false;
    double turnBy = 0;
    bool reset = false;  // double-click
};
KnobInput ReadKnob();

}  // namespace bw
