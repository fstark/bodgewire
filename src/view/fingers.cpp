#include "view/fingers.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace bw {

void Fingers::push(PointerChange change) {
    if (!change.present) {
        change.x = change.y = 0;
    }
    if (change != last_) {
        changes_.push_back(change);
        last_ = change;
    }
}

float Fingers::distance() const { return std::hypot(second_.x - first_.x, second_.y - first_.y); }

void Fingers::press(const FingerEvent& event, double now) {
    state_ = State::One;
    first_ = {event.finger, event.x, event.y};
    downX_ = event.x;
    downY_ = event.y;
    downAt_ = now;
    typing_ = event.typing;
    dragged_ = hold_ = false;
    lingerUntil_.reset();
    pressAt_ = changes_.size();
    beforePress_ = last_;
    push({true, event.x, event.y, false});  // there first: ImGui hovers before it presses
    push({true, event.x, event.y, true});
}

void Fingers::voidPress() {
    if (pressAt_) {
        // ImGui never saw it: it never does.
        changes_.resize(*pressAt_);
        last_ = beforePress_;
        pressAt_.reset();
    } else {
        cancelled_ = true;
        push({true, first_.x, first_.y, false});
    }
    hold_ = false;
}

void Fingers::event(const FingerEvent& event, double now) {
    using Kind = FingerEvent::Kind;
    switch (state_) {
    case State::None:
        if (event.kind == Kind::Down) {
            press(event, now);
        }
        break;
    case State::One:
        if (event.kind == Kind::Down) {
            if (typing_ && event.typing) {
                // Two thumbs typing: the first key up, the second down.
                push({true, first_.x, first_.y, false});
                pressAt_.reset();
                press(event, now);
                break;
            }
            voidPress();
            state_ = State::Two;
            second_ = {event.finger, event.x, event.y};
            startDistance_ = lastDistance_ = distance();
            startCentreX_ = lastCentreX_ = (first_.x + second_.x) / 2;
            startCentreY_ = lastCentreY_ = (first_.y + second_.y) / 2;
            pinching_ = panning_ = false;
        } else if (event.finger == first_.id) {
            first_.x = event.x;
            first_.y = event.y;
            if (!hold_ && std::hypot(event.x - downX_, event.y - downY_) > kTapSlop) {
                dragged_ = true;
            }
            switch (event.kind) {
            case Kind::Move:
                push({true, event.x, event.y, true});
                break;
            case Kind::Up:
                push({true, event.x, event.y, false});
                state_ = State::None;
                if (dragged_) {
                    push({false, 0, 0, false});
                } else {
                    lingerUntil_ = now + kLingerTime;
                }
                hold_ = false;
                break;
            case Kind::Cancel:
                voidPress();
                push({false, 0, 0, false});
                state_ = State::None;
                break;
            case Kind::Down:
                break;
            }
        }
        break;
    case State::Two:
        if (event.kind == Kind::Down || (event.finger != first_.id && event.finger != second_.id)) {
            break;  // a third finger
        }
        if (event.kind == Kind::Move) {
            Finger& moved = event.finger == first_.id ? first_ : second_;
            moved.x = event.x;
            moved.y = event.y;
        } else {
            ending_ = event.finger == first_.id ? second_.id : first_.id;
            state_ = State::Ending;
            push({false, 0, 0, false});
        }
        break;
    case State::Ending:
        if ((event.kind == Kind::Up || event.kind == Kind::Cancel) && event.finger == ending_) {
            state_ = State::None;
        }
        break;
    }
}

FingerFrame Fingers::frame(double now) {
    FingerFrame frame;
    switch (state_) {
    case State::None:
        if (lingerUntil_ && now >= *lingerUntil_) {
            lingerUntil_.reset();
            push({false, 0, 0, false});
        }
        break;
    case State::One:
        if (!hold_ && !dragged_ && now - downAt_ >= kHoldTime) {
            hold_ = true;
        }
        frame.hold = hold_;
        break;
    case State::Two: {
        const float centreX = (first_.x + second_.x) / 2;
        const float centreY = (first_.y + second_.y) / 2;
        const float apart = distance();
        frame.two = true;
        // Past the slop, measured from there on: no jump when a gesture begins.
        if (!pinching_ && std::abs(apart - startDistance_) > kPinchSlop) {
            pinching_ = true;
            lastDistance_ = startDistance_ + (apart > startDistance_ ? kPinchSlop : -kPinchSlop);
        }
        if (!panning_ &&
            std::hypot(centreX - startCentreX_, centreY - startCentreY_) > kPinchSlop) {
            panning_ = true;
            lastCentreX_ = centreX;
            lastCentreY_ = centreY;
        }
        if (pinching_ && lastDistance_ > 0 && apart > 0) {
            frame.pinch = apart / lastDistance_;
            lastDistance_ = apart;
        }
        if (panning_) {
            frame.panX = centreX - lastCentreX_;
            frame.panY = centreY - lastCentreY_;
            lastCentreX_ = centreX;
            lastCentreY_ = centreY;
        }
        push({true, centreX, centreY, false});
        break;
    }
    case State::Ending:
        break;
    }
    frame.cancelled = std::exchange(cancelled_, false);
    frame.pointer = std::move(changes_);
    changes_.clear();
    pressAt_.reset();
    return frame;
}

void Fingers::reset() {
    if (state_ == State::One) {
        voidPress();
    }
    state_ = State::None;
    lingerUntil_.reset();
    push({false, 0, 0, false});
}

void Fingers::mouse() {
    if (state_ == State::None && lingerUntil_) {
        lingerUntil_.reset();
        last_ = {false, 0, 0, false};  // the mouse's now: nothing of ours left to park
    }
}

std::vector<TimedFingerEvent> ScriptEvents(std::span<const FingerScript> scripts) {
    using Kind = FingerEvent::Kind;
    constexpr double kFirst = 0.3, kApart = 0.6, kTap = 0.05, kSecondTap = 0.15, kMove = 0.3;
    constexpr int kSteps = 12;
    constexpr float kPinchHalf = 100;  // px from the centre to each finger at the start
    std::vector<TimedFingerEvent> events;
    for (std::size_t k = 0; k < scripts.size(); ++k) {
        const FingerScript& s = scripts[k];
        const double t = kFirst + kApart * static_cast<double>(k);
        const auto add = [&](double at, Kind kind, std::uint64_t finger, float x, float y) {
            events.push_back({at, {kind, finger, x, y}});
        };
        switch (s.kind) {
        case FingerScript::Kind::Tap:
            add(t, Kind::Down, 1, s.x, s.y);
            add(t + kTap, Kind::Up, 1, s.x, s.y);
            break;
        case FingerScript::Kind::DoubleTap:
            add(t, Kind::Down, 1, s.x, s.y);
            add(t + kTap, Kind::Up, 1, s.x, s.y);
            add(t + kSecondTap, Kind::Down, 1, s.x, s.y);
            add(t + kSecondTap + kTap, Kind::Up, 1, s.x, s.y);
            break;
        case FingerScript::Kind::Hold:
            add(t, Kind::Down, 1, s.x, s.y);
            break;
        case FingerScript::Kind::Drag:
            add(t, Kind::Down, 1, s.x, s.y);
            for (int i = 1; i <= kSteps; ++i) {
                const float share = static_cast<float>(i) / kSteps;
                add(t + kTap + kMove * share, Kind::Move, 1, s.x + s.dx * share,
                    s.y + s.dy * share);
            }
            add(t + kTap + kMove + kTap, Kind::Up, 1, s.x + s.dx, s.y + s.dy);
            break;
        case FingerScript::Kind::Pinch: {
            add(t, Kind::Down, 1, s.x - kPinchHalf, s.y);
            add(t + 0.02, Kind::Down, 2, s.x + kPinchHalf, s.y);
            float half = kPinchHalf;
            for (int i = 1; i <= kSteps; ++i) {
                const float share = static_cast<float>(i) / kSteps;
                half = kPinchHalf * (1 + (s.factor - 1) * share);
                add(t + kTap + kMove * share, Kind::Move, 1, s.x - half, s.y);
                add(t + kTap + kMove * share, Kind::Move, 2, s.x + half, s.y);
            }
            add(t + kTap + kMove + kTap, Kind::Up, 2, s.x + half, s.y);
            add(t + kTap + kMove + kTap + 0.02, Kind::Up, 1, s.x - half, s.y);
            break;
        }
        }
    }
    std::ranges::stable_sort(events, {}, &TimedFingerEvent::at);
    return events;
}

}  // namespace bw
