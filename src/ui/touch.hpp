#pragma once

#include <span>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "view/fingers.hpp"

namespace bw {

// The fingers' glue (P8f): SDL's finger events in, ImGui's one pointer and the frame's gestures
// out. A finger differs from the mouse only here and in the tolerances this sets (TOUCH.md §R2).

// Once, before SDL_Init: SDL no longer makes a mouse from the first finger, nor fingers from a
// pen (a pen is a mouse only).
void TouchInit();
// Each screen (the Launcher, a bench), after its ImGui context is made: fresh fingers, the
// script's clock at 0. `emulated` (--touch): the mouse stands in for one finger. `script`: px of
// `window`.
void TouchStart(SDL_Window* window, bool emulated, std::span<const FingerScript> script);
// In the event loop, first: true for an event that is the fingers' and not ImGui's backend's.
bool TouchEvent(const SDL_Event& event);
// After the backend's NewFrame, before ImGui::NewFrame: the script's due events, the pointer
// queued for ImGui, the tolerances for the pointer in use.
void TouchNewFrame();
// This frame's gestures; all at rest with a mouse.
const FingerFrame& Touch();
// The pointer in use is a finger: the last pointer event ImGui took came from the touch screen.
bool FingerPointer();
// The last press was cancelled by a second finger: its release is no click. For what acts at a
// release by its own test (a link); an ImGui item has already let go.
bool PressCancelled();
// Inside a window whose items are typed on with two fingers (the keyboards), each frame: a second
// finger there takes over from the first, and nothing is pinched.
void TouchTypingZone();

// Each frame, last: with a finger in use and the window taller than wide, a card over everything
// asks to turn the tablet (the benches are laid out for landscape).
void TurnNotice();

// The finger's reach around a control, px (ImGui's TouchExtraPadding while FingerPointer()).
inline constexpr float kFingerPadding = 6;

// For a scope: no finger padding around the items made in it (a keyboard's keys, items set edge
// to edge: the padding would move their borders).
struct NoFingerPadding {
    NoFingerPadding();
    ~NoFingerPadding();
    NoFingerPadding(const NoFingerPadding&) = delete;
    NoFingerPadding& operator=(const NoFingerPadding&) = delete;
    ImVec2 saved;
};

}  // namespace bw
