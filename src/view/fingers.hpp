#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bw {

// Touch as the UI's input (P8f): the fingers on the screen made into the one pointer ImGui knows
// and the gestures a mouse has not. No SDL, no ImGui: fed by ui/touch.cpp, or by a script.

// One finger's event, in window px.
struct FingerEvent {
    enum class Kind : std::uint8_t { Down, Move, Up, Cancel };  // Cancel: the system took it
    Kind kind;
    std::uint64_t finger;  // SDL's id
    float x, y;
    bool typing = false;  // Down: on a keyboard's keys (a typing zone)
};

inline constexpr float kTapSlop = 10;       // px a finger may move and still tap or hold
inline constexpr double kHoldTime = 0.4;    // s without moving: a hold
inline constexpr double kLingerTime = 2.5;  // s the pointer stays after a tap or a hold
// Two fingers: their distance or their centre must change by this much (px) before they pinch or
// pan; what comes after is measured from there (no jump).
inline constexpr float kPinchSlop = 10;

// A change of the one pointer ImGui sees, in order.
struct PointerChange {
    bool present;  // false: parked off the screen (x, y unused)
    float x, y;
    bool down;
    bool operator==(const PointerChange&) const = default;
};

// What the fingers are doing this frame.
struct FingerFrame {
    std::vector<PointerChange> pointer;  // since the last frame
    bool cancelled = false;  // this frame: the press under way is void (its release clicks nothing)
    bool hold = false;       // one finger, down kHoldTime within kTapSlop: until it lifts
    bool two = false;        // two fingers down: the pointer at their centre, up
    float pinch = 1;         // their distance over last frame's, once past kPinchSlop
    float panX = 0, panY = 0;  // their centre's move since last frame, px, once past kPinchSlop
};

// One finger is the pointer: there and down while it touches; after a tap or a hold it lingers
// kLingerTime where it lifted (the tap's highlight and tooltip show), then is parked off the
// screen; after a drag it is parked at once. A second finger cancels the first's press: the
// frame is `cancelled`, the pointer goes up where it is (never off the screen while down: a
// hand-written drag would read that place) and then hovers at the two fingers' centre while they
// pinch and pan; the first to lift ends it, the other is ignored until it lifts. A third finger
// is ignored. On a keyboard (both fingers down in a typing zone) there is no gesture: the second
// finger takes over from the first, so two thumbs type.
class Fingers {
public:
    void event(const FingerEvent& event, double now);  // seconds, any origin
    FingerFrame frame(double now);                     // once a frame, after its events
    // No finger is down any more (focus lost: a lift never sent): the press void, the pointer
    // parked.
    void reset();
    // A real mouse moved: the pointer is its own, with no linger left to park it.
    void mouse();

private:
    enum class State : std::uint8_t { None, One, Two, Ending };
    struct Finger {
        std::uint64_t id = 0;
        float x = 0, y = 0;
    };
    void push(PointerChange change);
    void press(const FingerEvent& event, double now);
    void voidPress();  // the one finger's press: never sent, or cancelled and up where it is
    float distance() const;

    State state_ = State::None;
    Finger first_, second_;
    float downX_ = 0, downY_ = 0;
    double downAt_ = 0;
    bool typing_ = false;  // the one finger went down in a typing zone
    bool dragged_ = false, hold_ = false;
    std::optional<double> lingerUntil_;
    // Two fingers: where they began, and last frame's, once past the slop.
    float startDistance_ = 0, startCentreX_ = 0, startCentreY_ = 0;
    float lastDistance_ = 0, lastCentreX_ = 0, lastCentreY_ = 0;
    bool pinching_ = false, panning_ = false;
    std::uint64_t ending_ = 0;  // the finger still down after a two-finger gesture
    bool cancelled_ = false;    // for the next frame()
    PointerChange last_{false, 0, 0, false};
    // The one finger's press still waiting in changes_ (no frame() since): where it starts, and
    // the pointer before it.
    std::optional<std::size_t> pressAt_;
    PointerChange beforePress_{false, 0, 0, false};
    std::vector<PointerChange> changes_;
};

// A scripted gesture (bw_ui --tap, --double-tap, --hold, --drag, --pinch), window px.
struct FingerScript {
    enum class Kind : std::uint8_t { Tap, DoubleTap, Hold, Drag, Pinch };
    Kind kind;
    float x, y;
    float dx = 0, dy = 0;  // Drag: how far
    float factor = 1;      // Pinch: the fingers' distance at the end over the start's (200 px)
    bool operator==(const FingerScript&) const = default;
};

struct TimedFingerEvent {
    double at;  // s from the screen's start
    FingerEvent event;
};

// Every script's events in time order: the k-th starts at 0.3 s + 0.6 s × k; a tap lifts 50 ms
// later; a double tap is two taps 150 ms apart; a drag and a pinch move in 12 steps over 0.3 s,
// then lift; a hold never lifts.
std::vector<TimedFingerEvent> ScriptEvents(std::span<const FingerScript> scripts);

}  // namespace bw
