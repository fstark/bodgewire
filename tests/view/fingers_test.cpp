#include "view/fingers.hpp"

#include <array>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

using Kind = FingerEvent::Kind;

constexpr PointerChange kParked{false, 0, 0, false};

PointerChange At(float x, float y, bool down) { return {true, x, y, down}; }

// The pointer's changes of one frame.
std::vector<PointerChange> Changes(Fingers& fingers, double now) {
    return fingers.frame(now).pointer;
}

}  // namespace

TEST_CASE("fingers: a tap is the pointer there, down, up, then lingering", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 200}, 1.0);
    CHECK(Changes(fingers, 1.0) ==
          std::vector{At(100, 200, false), At(100, 200, true)});  // hovered before pressed
    fingers.event({Kind::Up, 1, 100, 200}, 1.05);
    CHECK(Changes(fingers, 1.05) == std::vector{At(100, 200, false)});
    CHECK(Changes(fingers, 3.5).empty());  // still there: its tooltip shows
    CHECK(Changes(fingers, 1.05 + kLingerTime) == std::vector{kParked});
    CHECK(Changes(fingers, 10).empty());
}

TEST_CASE("fingers: a down and an up in one frame are still three changes", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 10, 20}, 1.0);
    fingers.event({Kind::Up, 1, 10, 20}, 1.0);
    CHECK(Changes(fingers, 1.0) ==
          std::vector{At(10, 20, false), At(10, 20, true), At(10, 20, false)});
}

TEST_CASE("fingers: a slide within the slop is a tap, past it a drag that parks", "[view]") {
    Fingers tap;
    tap.event({Kind::Down, 1, 100, 100}, 0);
    tap.event({Kind::Move, 1, 100 + kTapSlop - 1, 100}, 0.05);
    tap.event({Kind::Up, 1, 100 + kTapSlop - 1, 100}, 0.1);
    CHECK(Changes(tap, 0.1).back() == At(100 + kTapSlop - 1, 100, false));  // lingering

    Fingers drag;
    drag.event({Kind::Down, 1, 100, 100}, 0);
    drag.event({Kind::Move, 1, 100, 100 + kTapSlop + 1}, 0.05);
    drag.event({Kind::Up, 1, 100, 100 + kTapSlop + 1}, 0.1);
    const auto changes = Changes(drag, 0.1);
    REQUIRE(changes.size() >= 2);
    CHECK(changes[changes.size() - 2] == At(100, 100 + kTapSlop + 1, false));  // released there
    CHECK(changes.back() == kParked);                                          // no hover left
}

TEST_CASE("fingers: a hold after kHoldTime, kept while sliding, lingering after", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 50, 50}, 0);
    CHECK_FALSE(fingers.frame(kHoldTime - 0.01).hold);
    CHECK(fingers.frame(kHoldTime).hold);
    fingers.event({Kind::Move, 1, 150, 90}, 0.6);  // far past the slop: still a hold
    CHECK(fingers.frame(0.6).hold);
    fingers.event({Kind::Up, 1, 150, 90}, 0.7);
    const FingerFrame after = fingers.frame(0.7);
    CHECK_FALSE(after.hold);
    CHECK(after.pointer.back() == At(150, 90, false));  // lingers: not parked
    CHECK(Changes(fingers, 0.7 + kLingerTime) == std::vector{kParked});
}

TEST_CASE("fingers: a finger that moved first never holds", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 50, 50}, 0);
    fingers.event({Kind::Move, 1, 50, 50 + kTapSlop + 5}, 0.1);
    CHECK_FALSE(fingers.frame(1.0).hold);
}

TEST_CASE("fingers: a new tap during the linger", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 10, 10}, 0);
    fingers.event({Kind::Up, 1, 10, 10}, 0.05);
    (void)fingers.frame(0.05);
    fingers.event({Kind::Down, 2, 300, 40}, 1.0);
    CHECK(Changes(fingers, 1.0) == std::vector{At(300, 40, false), At(300, 40, true)});
    fingers.event({Kind::Up, 2, 300, 40}, 1.05);
    (void)fingers.frame(1.05);
    CHECK(Changes(fingers, 0.05 + kLingerTime).empty());  // the first tap's linger is gone
    CHECK(Changes(fingers, 1.05 + kLingerTime) == std::vector{kParked});
}

TEST_CASE("fingers: a second finger cancels the first", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    CHECK_FALSE(fingers.frame(0).cancelled);
    fingers.event({Kind::Move, 1, 104, 100}, 0.05);
    fingers.event({Kind::Down, 2, 204, 100}, 0.1);
    const FingerFrame frame = fingers.frame(0.1);
    // The press void; up where the finger is, never off the screen while down; then hovering at
    // the centre.
    CHECK(frame.cancelled);
    CHECK(frame.pointer ==
          std::vector{At(104, 100, true), At(104, 100, false), At(154, 100, false)});
    CHECK(frame.two);
    CHECK_FALSE(frame.hold);
    CHECK(frame.pinch == Catch::Approx(1));
    CHECK_FALSE(fingers.frame(0.2).cancelled);  // one frame only
}

TEST_CASE("fingers: two downs in one frame leave no press", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    fingers.event({Kind::Down, 2, 200, 100}, 0);
    const FingerFrame frame = fingers.frame(0);
    CHECK_FALSE(frame.cancelled);  // nothing was pressed for ImGui
    CHECK(frame.pointer == std::vector{At(150, 100, false)});
    CHECK(frame.two);
}

TEST_CASE("fingers: a pinch's ratio and a two-finger drag, past the slop", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    fingers.event({Kind::Down, 2, 200, 100}, 0);
    (void)fingers.frame(0);
    fingers.event({Kind::Move, 2, 200 + kPinchSlop - 2, 100 + 4}, 0.05);  // inside the slop
    FingerFrame frame = fingers.frame(0.05);
    CHECK(frame.pinch == Catch::Approx(1));
    CHECK(frame.panX == Catch::Approx(0));
    fingers.event({Kind::Move, 1, 50, 100}, 0.1);  // 100 apart → 200 apart, the centre kept
    fingers.event({Kind::Move, 2, 250, 100}, 0.1);
    frame = fingers.frame(0.1);
    CHECK(frame.pinch == Catch::Approx(200.0 / (100 + kPinchSlop)));  // from the slop's edge
    CHECK(frame.panX == Catch::Approx(0));
    CHECK(frame.panY == Catch::Approx(0));
    fingers.event({Kind::Move, 1, 50, 150}, 0.2);  // both 50 down: past the slop from here
    fingers.event({Kind::Move, 2, 250, 150}, 0.2);
    frame = fingers.frame(0.2);
    CHECK(frame.pinch == Catch::Approx(1));
    CHECK(frame.panY == Catch::Approx(0));         // the frame it begins: nothing yet
    fingers.event({Kind::Move, 1, 60, 180}, 0.3);  // both 10 right, 30 down
    fingers.event({Kind::Move, 2, 260, 180}, 0.3);
    frame = fingers.frame(0.3);
    CHECK(frame.panX == Catch::Approx(10));
    CHECK(frame.panY == Catch::Approx(30));
    CHECK(frame.pointer == std::vector{At(160, 180, false)});
    frame = fingers.frame(0.4);  // nothing moved
    CHECK(frame.pinch == Catch::Approx(1));
    CHECK(frame.panX == Catch::Approx(0));
}

TEST_CASE("fingers: the first of two to lift ends it, the other is ignored", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    fingers.event({Kind::Down, 2, 200, 100}, 0);
    (void)fingers.frame(0);
    fingers.event({Kind::Up, 1, 100, 100}, 0.5);
    FingerFrame frame = fingers.frame(0.5);
    CHECK_FALSE(frame.two);
    CHECK(frame.pointer == std::vector{kParked});
    fingers.event({Kind::Move, 2, 400, 400}, 0.6);
    fingers.event({Kind::Up, 2, 400, 400}, 0.7);
    CHECK(Changes(fingers, 0.7).empty());         // no tap, no drag
    fingers.event({Kind::Down, 3, 20, 20}, 1.0);  // a new touch works again
    CHECK(Changes(fingers, 1.0) == std::vector{At(20, 20, false), At(20, 20, true)});
}

TEST_CASE("fingers: a third finger is ignored", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    fingers.event({Kind::Down, 2, 200, 100}, 0);
    (void)fingers.frame(0);
    fingers.event({Kind::Down, 3, 300, 300}, 0.1);
    fingers.event({Kind::Move, 3, 500, 500}, 0.2);
    fingers.event({Kind::Up, 3, 500, 500}, 0.3);
    const FingerFrame frame = fingers.frame(0.3);
    CHECK(frame.two);
    CHECK(frame.pinch == Catch::Approx(1));
    CHECK(frame.pointer.empty());
}

TEST_CASE("fingers: a finger the system takes back is a void press", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100}, 0);
    (void)fingers.frame(0);
    fingers.event({Kind::Cancel, 1, 100, 100}, 0.2);
    const FingerFrame frame = fingers.frame(0.2);
    CHECK(frame.cancelled);
    CHECK(frame.pointer == std::vector{At(100, 100, false), kParked});
    fingers.event({Kind::Down, 2, 30, 30}, 1.0);  // not a second finger: the first is gone
    CHECK(Changes(fingers, 1.0) == std::vector{At(30, 30, false), At(30, 30, true)});
}

TEST_CASE("fingers: on a keyboard the second key takes over", "[view]") {
    Fingers fingers;
    fingers.event({Kind::Down, 1, 100, 100, true}, 0);
    (void)fingers.frame(0);
    fingers.event({Kind::Down, 2, 160, 100, true}, 0.1);
    FingerFrame frame = fingers.frame(0.1);
    CHECK_FALSE(frame.two);  // no gesture
    CHECK_FALSE(frame.cancelled);
    CHECK(frame.pointer ==
          std::vector{At(100, 100, false), At(160, 100, false), At(160, 100, true)});
    fingers.event({Kind::Up, 1, 100, 100, true}, 0.15);  // the first thumb lifts: nothing
    CHECK(Changes(fingers, 0.15).empty());
    fingers.event({Kind::Up, 2, 160, 100, true}, 0.2);
    CHECK(Changes(fingers, 0.2) == std::vector{At(160, 100, false)});

    Fingers mixed;  // the first on the Board, the second on a key: a gesture as anywhere
    mixed.event({Kind::Down, 1, 100, 100, false}, 0);
    (void)mixed.frame(0);
    mixed.event({Kind::Down, 2, 160, 100, true}, 0.1);
    CHECK(mixed.frame(0.1).two);
}

TEST_CASE("fingers: reset and a real mouse", "[view]") {
    Fingers held;
    held.event({Kind::Down, 1, 100, 100}, 0);
    (void)held.frame(0);
    held.reset();  // its lift never came
    FingerFrame frame = held.frame(0.5);
    CHECK(frame.cancelled);
    CHECK(frame.pointer == std::vector{At(100, 100, false), kParked});
    held.event({Kind::Down, 2, 5, 5}, 1.0);
    CHECK(Changes(held, 1.0) == std::vector{At(5, 5, false), At(5, 5, true)});

    Fingers lingering;
    lingering.event({Kind::Down, 1, 10, 10}, 0);
    lingering.event({Kind::Up, 1, 10, 10}, 0.05);
    (void)lingering.frame(0.05);
    lingering.mouse();  // the mouse's pointer now: nothing parks it
    CHECK(Changes(lingering, 0.05 + kLingerTime).empty());
    lingering.event({Kind::Down, 2, 10, 10}, 5.0);  // the same place: told again
    CHECK(Changes(lingering, 5.0) == std::vector{At(10, 10, false), At(10, 10, true)});
}

TEST_CASE("fingers: scripted gestures", "[view]") {
    const std::array scripts{
        FingerScript{FingerScript::Kind::Tap, 100, 200},
        FingerScript{FingerScript::Kind::Tap, 100, 200},
        FingerScript{FingerScript::Kind::Pinch, 640, 400, 0, 0, 3},
        FingerScript{FingerScript::Kind::Hold, 10, 20},
    };
    const std::vector<TimedFingerEvent> events = ScriptEvents(scripts);
    REQUIRE(events.size() >= 5);
    // The first tap: down at 0.3 s, up 50 ms later; the second 0.6 s after it.
    CHECK(events[0].at == Catch::Approx(0.3));
    CHECK(events[0].event.kind == Kind::Down);
    CHECK(events[1].at == Catch::Approx(0.35));
    CHECK(events[1].event.kind == Kind::Up);
    CHECK(events[2].at == Catch::Approx(0.9));
    for (std::size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i - 1].at <= events[i].at);
    }
    // Played through the fingers: the pinch ends 3 times as wide as it began (200 px), less
    // the slop it had to pass.
    Fingers fingers;
    float zoom = 1;
    bool holding = false;
    std::size_t next = 0;
    for (double now = 0; now < 3.0; now += 1.0 / 60) {
        while (next < events.size() && events[next].at <= now) {
            fingers.event(events[next].event, events[next].at);
            ++next;
        }
        const FingerFrame frame = fingers.frame(now);
        zoom *= frame.pinch;
        holding = frame.hold;
    }
    CHECK(zoom == Catch::Approx(600.0 / (200 + kPinchSlop)));
    CHECK(holding);  // the hold, started last, never lifts
}

TEST_CASE("fingers: a scripted double tap and drag", "[view]") {
    const std::array scripts{
        FingerScript{FingerScript::Kind::DoubleTap, 5, 6},
        FingerScript{FingerScript::Kind::Drag, 100, 100, 0, -80},
    };
    const std::vector<TimedFingerEvent> events = ScriptEvents(scripts);
    REQUIRE(events.size() == 4 + 14);
    CHECK(events[2].at - events[0].at == Catch::Approx(0.15));  // within a double tap's time
    CHECK(events[4].at == Catch::Approx(0.9));
    CHECK(events.back().event.kind == Kind::Up);
    CHECK(events.back().event.y == Catch::Approx(20));
}
