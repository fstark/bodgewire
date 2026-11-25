#include "ui/touch.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <vector>

#include <imgui_internal.h>

namespace bw {

namespace {

using Clock = std::chrono::steady_clock;

// A mouse's tolerances: ImGui's defaults.
constexpr float kMouseDragThreshold = 6;
constexpr float kMouseDoubleClickTime = 0.30f, kMouseDoubleClickDist = 6;
constexpr float kFingerDoubleTapTime = 0.35f, kFingerDoubleTapDist = 24;
constexpr std::uint64_t kMouseFinger = 1;  // the emulated finger's id

Fingers fingers_;
FingerFrame frame_;
SDL_Window* window_ = nullptr;
bool emulated_ = false;
bool mouseDown_ = false;  // emulated: the left button is down
std::vector<TimedFingerEvent> script_;
std::size_t scriptNext_ = 0;
Clock::time_point start_;
PointerChange shown_{false, 0, 0, false};  // what ImGui was last told
bool ours_ = false;                        // the pointer is the fingers': told again each frame
bool cancelled_ = false;                   // the last press was cancelled, until the next one
std::vector<ImRect> zones_, zonesNow_;     // the typing zones: last frame's, this frame's

double Now() { return std::chrono::duration<double>(Clock::now() - start_).count(); }

// Window px to ImGui's: the desktop's with viewports enabled, as its SDL3 backend does.
ImVec2 InImGui(SDL_Window* window, float x, float y) {
    if (window != nullptr && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
        int wx = 0, wy = 0;
        SDL_GetWindowPosition(window, &wx, &wy);
        x += static_cast<float>(wx);
        y += static_cast<float>(wy);
    }
    return {x, y};
}

void Feed(FingerEvent::Kind kind, std::uint64_t finger, ImVec2 at, double now) {
    bool typing = false;
    if (kind == FingerEvent::Kind::Down) {
        for (const ImRect& zone : zones_) {
            typing = typing || zone.Contains(at);
        }
    }
    fingers_.event({kind, finger, at.x, at.y, typing}, now);
    ours_ = true;
}

void Tell(const PointerChange& change) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMousePosEvent(change.present ? change.x : -FLT_MAX, change.present ? change.y : -FLT_MAX);
    if (change.down != shown_.down) {
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, change.down);
        cancelled_ = cancelled_ && !change.down;  // a new press
    }
    shown_ = change;
}

// A real mouse's event: the pointer is its own again.
void MouseTookOver() {
    if (ours_) {
        ours_ = false;
        fingers_.mouse();
        shown_ = {false, 0, 0, shown_.down};
    }
}

}  // namespace

void TouchInit() {
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_PEN_TOUCH_EVENTS, "0");
}

void TouchStart(SDL_Window* window, bool emulated, std::span<const FingerScript> script) {
    fingers_ = Fingers{};
    frame_ = FingerFrame{};
    window_ = window;
    emulated_ = emulated;
    mouseDown_ = false;
    script_ = ScriptEvents(script);
    scriptNext_ = 0;
    start_ = Clock::now();
    shown_ = {false, 0, 0, false};
    cancelled_ = false;
    ours_ = emulated || !script.empty();
    zones_.clear();
    zonesNow_.clear();
}

bool TouchEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        // A screen's fingers only: not a trackpad's (indirect, no window), nor the ones SDL
        // makes from a mouse or a pen.
        SDL_Window* window = SDL_GetWindowFromID(event.tfinger.windowID);
        if (window == nullptr || event.tfinger.touchID == SDL_MOUSE_TOUCHID ||
            event.tfinger.touchID == SDL_PEN_TOUCHID ||
            SDL_GetTouchDeviceType(event.tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) {
            return true;
        }
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        const FingerEvent::Kind kind =
            event.type == SDL_EVENT_FINGER_DOWN     ? FingerEvent::Kind::Down
            : event.type == SDL_EVENT_FINGER_MOTION ? FingerEvent::Kind::Move
            : event.type == SDL_EVENT_FINGER_UP     ? FingerEvent::Kind::Up
                                                    : FingerEvent::Kind::Cancel;
        Feed(kind, event.tfinger.fingerID,
             InImGui(window, event.tfinger.x * static_cast<float>(w),
                     event.tfinger.y * static_cast<float>(h)),
             Now());
        return true;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (ours_) {
            fingers_.reset();  // a lift the system may never send
            mouseDown_ = false;
        }
        return false;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!emulated_) {
            MouseTookOver();
            return false;
        }
        const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (event.button.button == SDL_BUTTON_LEFT && down != mouseDown_) {
            mouseDown_ = down;
            Feed(
                down ? FingerEvent::Kind::Down : FingerEvent::Kind::Up, kMouseFinger,
                InImGui(SDL_GetWindowFromID(event.button.windowID), event.button.x, event.button.y),
                Now());
        }
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (!emulated_) {
            MouseTookOver();
            return false;
        }
        if (mouseDown_) {
            Feed(
                FingerEvent::Kind::Move, kMouseFinger,
                InImGui(SDL_GetWindowFromID(event.motion.windowID), event.motion.x, event.motion.y),
                Now());
        }
        return true;
    case SDL_EVENT_MOUSE_WHEEL:
        return emulated_;  // a finger has no wheel
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        return emulated_ || !script_.empty();  // the backend would move or park the pointer
    default:
        return false;
    }
}

void TouchNewFrame() {
    const double now = Now();
    while (scriptNext_ < script_.size() && script_[scriptNext_].at <= now) {
        const TimedFingerEvent& timed = script_[scriptNext_++];
        Feed(timed.event.kind, timed.event.finger, InImGui(window_, timed.event.x, timed.event.y),
             timed.at);
    }
    frame_ = fingers_.frame(now);
    zones_ = std::move(zonesNow_);
    zonesNow_.clear();
    ImGuiIO& io = ImGui::GetIO();
    if (frame_.cancelled) {
        // The press under way lets go of its item, and of the window it was moving, without a
        // click; BoardView and TimeCanvas drop theirs on Touch().cancelled.
        ImGui::ClearActiveID();
        ImGui::StopMouseMovingWindow();
        cancelled_ = true;
    }
    for (const PointerChange& change : frame_.pointer) {
        Tell(change);
    }
    if (ours_ && frame_.pointer.empty()) {
        // Told again: on a desktop ImGui's backend gives it the real cursor's place each frame.
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(shown_.present ? shown_.x : -FLT_MAX,
                            shown_.present ? shown_.y : -FLT_MAX);
    }
    const bool finger = ours_ || FingerPointer();
    io.MouseDragThreshold = finger ? kTapSlop : kMouseDragThreshold;
    io.MouseDoubleClickTime = finger ? kFingerDoubleTapTime : kMouseDoubleClickTime;
    io.MouseDoubleClickMaxDist = finger ? kFingerDoubleTapDist : kMouseDoubleClickDist;
    ImGui::GetStyle().TouchExtraPadding =
        finger ? ImVec2{kFingerPadding, kFingerPadding} : ImVec2{0, 0};
}

const FingerFrame& Touch() { return frame_; }

bool FingerPointer() { return ImGui::GetIO().MouseSource == ImGuiMouseSource_TouchScreen; }

bool PressCancelled() { return cancelled_; }

void TouchTypingZone() {
    const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    zonesNow_.push_back(window->Rect());
}

void TurnNotice() {
    const ImGuiViewport* main = ImGui::GetMainViewport();
    if (!(ours_ || FingerPointer()) || main->Size.y <= main->Size.x) {
        return;
    }
    constexpr const char* kTitle = "Turn the tablet";
    constexpr const char* kLine = "Bodgewire is laid out for landscape.";
    constexpr float kTitleScale = 1.5f, kPad = 24;
    ImDrawList& list = *ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const ImVec2 title = font->CalcTextSizeA(size * kTitleScale, FLT_MAX, 0, kTitle);
    const ImVec2 line = ImGui::CalcTextSize(kLine);
    const ImVec2 card{std::max(title.x, line.x) + 2 * kPad, title.y + line.y + 2 * kPad + 10};
    const ImVec2 min{main->Pos.x + (main->Size.x - card.x) / 2,
                     main->Pos.y + (main->Size.y - card.y) / 2};
    list.AddRectFilled(main->Pos, {main->Pos.x + main->Size.x, main->Pos.y + main->Size.y},
                       IM_COL32(0, 0, 0, 200));
    list.AddRectFilled(min, {min.x + card.x, min.y + card.y}, IM_COL32(17, 17, 17, 255), 8.0f);
    list.AddRect(min, {min.x + card.x, min.y + card.y}, IM_COL32(102, 255, 255, 255), 8.0f);
    list.AddText(font, size * kTitleScale, {min.x + (card.x - title.x) / 2, min.y + kPad},
                 IM_COL32(102, 255, 255, 255), kTitle);
    list.AddText({min.x + (card.x - line.x) / 2, min.y + kPad + title.y + 10},
                 IM_COL32(221, 221, 221, 255), kLine);
}

NoFingerPadding::NoFingerPadding() : saved(ImGui::GetStyle().TouchExtraPadding) {
    ImGui::GetStyle().TouchExtraPadding = {0, 0};
}

NoFingerPadding::~NoFingerPadding() { ImGui::GetStyle().TouchExtraPadding = saved; }

}  // namespace bw
