#include "ui/gestures.hpp"

#include <cmath>

#include <imgui.h>

#include "ui/instrument_marks.hpp"
#include "ui/touch.hpp"
#include "view/fingers.hpp"

namespace bw {

namespace {

constexpr float kClickSlop = 3;      // px a mouse's press may move and still be a click
constexpr double kDragTurn = 200;    // px of vertical drag for a whole turn
constexpr double kWheelStep = 0.01;  // turn per wheel notch

}  // namespace

TimeCanvasInput TimeCanvas::read(float width, int divisions) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
    const ImGuiIO& io = ImGui::GetIO();
    const float x = io.MousePos.x;
    TimeCanvasInput input;
    if (ImGui::IsItemHovered()) {
        input.hover = x;
        input.zoom = WheelNotches(io.MouseWheel, wheelRest_);
        input.pan = -io.MouseWheelH * width / static_cast<float>(divisions) / 2;
        input.fit = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        input.clearMarker = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if (const FingerFrame& touch = Touch(); touch.two) {
            // Two fingers (P8f): a step each time their distance doubles or halves, about their
            // centre (the hover); the time panned with them.
            input.zoom += WheelNotches(std::log2(touch.pinch), pinchRest_);
            input.pan += touch.panX;
        }
    }
    if (ImGui::IsItemActivated()) {
        input.pressed = true;
        pressX_ = x;
        moved_ = false;
    }
    if (ImGui::IsItemActive()) {
        input.dragStep = io.MouseDelta.x;
        // The slop is the pointer's (P8f): a finger's tap slides more than a mouse's click.
        moved_ = moved_ || std::abs(x - pressX_) > (FingerPointer() ? kTapSlop : kClickSlop);
        if (moved_) {
            input.dragFromPress = x - pressX_;
        }
    }
    if (ImGui::IsItemDeactivated()) {
        input.released = true;
        if (!moved_ && !Touch().cancelled) {  // a second finger's cancel sets no Marker
            input.marker = x;
        }
    }
    return input;
}

KnobInput ReadKnob() {
    const ImGuiIO& io = ImGui::GetIO();
    KnobInput input;
    if (ImGui::IsItemActive()) {
        input.turnBy -= io.MouseDelta.y / kDragTurn;
    }
    if (ImGui::IsItemHovered()) {
        input.hovered = true;
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        input.turnBy += io.MouseWheel * kWheelStep;
        input.reset = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    }
    return input;
}

}  // namespace bw
