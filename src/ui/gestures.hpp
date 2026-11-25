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
    // A finger held still on the canvas, then slid (P8f): its x each frame; the cursor goes
    // there and stays (no pan, no marker).
    std::optional<float> scrub;
    float pressY = 0;  // where the press began, screen px: the lane or band it is on
    // A vertical drag's step, px to scroll down by (P8f, a canvas of rows that scroll): the drag
    // then gives no dragStep, no dragFromPress and no marker.
    float scrollY = 0;
};

class TimeCanvas {
public:
    // After the canvas's InvisibleButton: `width` px wide, `divisions` across. `markerX`: the
    // Marker's x when it is set, `top` the canvas's top: a click on the Marker's flag there
    // clears it (`clearMarker`) instead of moving it. `vertical`: the canvas's rows scroll, so
    // a drag is one or the other: nothing until it has moved past the slop, then the larger of
    // its two moves decides between the time's pan (`dragStep`) and `scrollY`.
    TimeCanvasInput read(float width, int divisions, std::optional<float> markerX = std::nullopt,
                         float top = 0, bool vertical = false);

private:
    float wheelRest_ = 0;
    float pinchRest_ = 0;  // of a pinch, not yet a whole step
    float pressX_ = 0, pressY_ = 0;
    bool moved_ = false;
    bool scrubbing_ = false;  // this press became a finger's hold
    bool scrolling_ = false;  // this press became a vertical drag
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
