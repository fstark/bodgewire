#include "ui/knob_window.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>

#include <imgui.h>

#include "ui/bench.hpp"
#include "view/bench_places.hpp"

namespace bw {

namespace {

constexpr double kKeyRate = 1.0;     // turn per second while a knob key is held
constexpr double kWheelStep = 0.01;  // turn per wheel notch
constexpr double kDragTurn = 200.0;  // px of vertical drag for a whole turn
constexpr float kSweep = 1.5f * std::numbers::pi_v<float>;   // 270°
constexpr float kStart = 0.75f * std::numbers::pi_v<float>;  // 0: lower left (screen angles)

constexpr ImU32 kBoxTop = IM_COL32(44, 40, 36, 255);
constexpr ImU32 kBoxBottom = IM_COL32(26, 24, 22, 255);
constexpr ImU32 kBoxEdge = IM_COL32(80, 74, 66, 255);
constexpr ImU32 kKnobBody = IM_COL32(16, 16, 17, 255);
constexpr ImU32 kKnobRidge = IM_COL32(60, 60, 64, 255);
constexpr ImU32 kKnobFace = IM_COL32(34, 34, 37, 255);
constexpr ImU32 kPointer = IM_COL32(240, 240, 235, 255);
constexpr ImU32 kTick = IM_COL32(190, 184, 170, 255);
constexpr ImU32 kLabel = IM_COL32(210, 204, 190, 255);
constexpr ImU32 kHover = IM_COL32(255, 255, 255, 30);
// The PC keys that turn a knob, printed under its reference as two small caps (P6g).
constexpr ImU32 kCapFace = IM_COL32(64, 61, 57, 255);
constexpr ImU32 kCapEdge = IM_COL32(18, 17, 16, 255);
constexpr ImU32 kCapLight = IM_COL32(96, 92, 86, 255);
constexpr ImU32 kCapPrint = IM_COL32(225, 220, 208, 255);
constexpr float kCapWidth = 20, kCapHeight = 17, kCapGap = 5;  // px at the first size

// Two caps centred on `centreX` from `top`: `Q` and `A`, or (`arrows`) up and down triangles.
void DrawKnobKeys(ImDrawList& list, float centreX, float top, float s, bool arrows) {
    const float w = kCapWidth * s, h = kCapHeight * s, gap = kCapGap * s;
    const float left = centreX - w - gap / 2;
    for (int i = 0; i < 2; ++i) {
        const ImVec2 a{left + static_cast<float>(i) * (w + gap), top};
        const ImVec2 b{a.x + w, a.y + h};
        list.AddRectFilled(a, b, kCapEdge, 3 * s);
        list.AddRectFilled({a.x + s, a.y + s}, {b.x - s, b.y - 2 * s}, kCapFace, 3 * s);
        list.AddLine({a.x + 3 * s, a.y + 1.5f * s}, {b.x - 3 * s, a.y + 1.5f * s}, kCapLight);
        const ImVec2 c{(a.x + b.x) / 2, (a.y + b.y) / 2 - 0.5f * s};
        if (arrows) {
            const float t = 4 * s;
            const float dir = i == 0 ? -1.0f : 1.0f;  // up, then down
            list.AddTriangleFilled({c.x - t, c.y - dir * t * 0.6f}, {c.x + t, c.y - dir * t * 0.6f},
                                   {c.x, c.y + dir * t * 0.6f}, kCapPrint);
        } else {
            const char* text = i == 0 ? "Q" : "A";
            const float size = 12 * s;
            const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text);
            list.AddText(ImGui::GetFont(), size, {c.x - extent.x / 2, c.y - extent.y / 2},
                         kCapPrint, text);
        }
    }
}

ImVec2 AtAngle(ImVec2 c, float r, float angle) {
    return {c.x + r * std::cos(angle), c.y + r * std::sin(angle)};
}

}  // namespace

const std::vector<std::string>& KnobWindow::knobs(const Session& session) {
    if (!knobs_) {
        knobs_ = session.knobs();
    }
    return *knobs_;
}

void KnobWindow::keys(Session& session) {
    const std::vector<std::string>& all = knobs(session);
    const ImGuiIO& io = ImGui::GetIO();
    if (all.empty() || io.WantTextInput) {
        return;
    }
    const auto turn = [&](const std::string& reference, ImGuiKey up, ImGuiKey down) {
        const int direction =
            static_cast<int>(ImGui::IsKeyDown(up)) - static_cast<int>(ImGui::IsKeyDown(down));
        if (direction != 0) {
            session.setKnob(reference,
                            session.knob(reference) + direction * kKeyRate * io.DeltaTime);
        }
    };
    // One knob: Up and Down. Two or more: Q and A turn the first, Up and Down the second.
    if (all.size() >= 2) {
        turn(all[0], ImGuiKey_Q, ImGuiKey_A);
        turn(all[1], ImGuiKey_UpArrow, ImGuiKey_DownArrow);
    } else {
        turn(all[0], ImGuiKey_UpArrow, ImGuiKey_DownArrow);
    }
}

void KnobWindow::draw(bool& open, Session& session) {
    const std::vector<std::string>& all = knobs(session);
    if (!open || all.empty()) {
        return;
    }
    const int count = static_cast<int>(all.size());
    const Extent first = FirstSize(BenchObject::Knob, BenchArea().extent(), 1, count);
    PlaceNext(BenchObject::Knob, {first.width, first.height});
    KeepAspect(first.height / first.width, first.width * 0.6f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBoxBottom);
    const bool visible =
        ImGui::Begin(kKnobWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!visible) {
        ImGui::End();
        return;
    }
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 at = ImGui::GetWindowPos();
    const float s = ImGui::GetWindowWidth() / first.width;  // scale from the first size
    const float boxW = kKnobBox.width * s, boxH = kKnobBox.height * s;
    const bool players = count >= 2;
    for (int i = 0; i < count; ++i) {
        const std::string& reference = all[static_cast<std::size_t>(i)];
        const ImVec2 min{at.x + static_cast<float>(i) * boxW, at.y};
        const ImVec2 max{min.x + boxW, min.y + boxH};
        const float inset = 3 * s;
        list.AddRectFilledMultiColor({min.x + inset, min.y + inset}, {max.x - inset, max.y - inset},
                                     kBoxTop, kBoxTop, kBoxBottom, kBoxBottom);
        list.AddRect({min.x + inset, min.y + inset}, {max.x - inset, max.y - inset}, kBoxEdge,
                     5 * s, 0, 1.0f);

        const ImVec2 c{min.x + boxW / 2, min.y + 70 * s};
        const float r = 44 * s;
        // Ticks at each end of the sweep and the middle.
        for (const float t : {0.0f, 0.5f, 1.0f}) {
            const float angle = kStart + t * kSweep;
            list.AddLine(AtAngle(c, r + 4 * s, angle), AtAngle(c, r + 10 * s, angle), kTick,
                         1.5f * s);
        }
        ImGui::SetCursorScreenPos({c.x - r, c.y - r});
        ImGui::PushID(i);
        ImGui::InvisibleButton("##knob", {2 * r, 2 * r});
        ImGui::PopID();
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive() && io.MouseDelta.y != 0.0f) {
            session.setKnob(reference, session.knob(reference) - io.MouseDelta.y / kDragTurn);
        }
        if (hovered) {
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
            if (io.MouseWheel != 0.0f) {
                session.setKnob(reference, session.knob(reference) + io.MouseWheel * kWheelStep);
            }
            const std::size_t upDown = players ? 1 : 0;  // the knob Up and Down turn
            const char* keys = players && i == 0
                                   ? "Turn the knob: drag up or down, mouse wheel, or Q and A"
                               : static_cast<std::size_t>(i) == upDown
                                   ? "Turn the knob: drag up or down, mouse wheel, or Up and Down"
                                   : "Turn the knob: drag up or down or mouse wheel";
            ImGui::SetTooltip("%s\n%s %.0f %%", keys, reference.c_str(),
                              session.knob(reference) * 100.0);
        }
        // The knob: a ridged rim, a face, the pointer.
        list.AddCircleFilled(c, r, kKnobBody, 48);
        constexpr int kRidges = 36;
        for (int k = 0; k < kRidges; ++k) {
            const float angle = 2 * std::numbers::pi_v<float> * static_cast<float>(k) / kRidges;
            list.AddLine(AtAngle(c, r * 0.86f, angle), AtAngle(c, r, angle), kKnobRidge, 1.0f);
        }
        list.AddCircleFilled(c, r * 0.8f, kKnobFace, 48);
        if (hovered) {
            list.AddCircleFilled(c, r, kHover, 48);
        }
        const float angle = kStart + static_cast<float>(session.knob(reference)) * kSweep;
        list.AddLine(AtAngle(c, r * 0.2f, angle), AtAngle(c, r * 0.95f, angle), kPointer, 3 * s);
        // Its reference under it, then the keys that turn it (the first two knobs only).
        const float size = 15 * s;
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, reference.c_str());
        list.AddText(ImGui::GetFont(), size, {c.x - text.x / 2, min.y + 140 * s - text.y / 2},
                     kLabel, reference.c_str());
        if (i < 2) {
            DrawKnobKeys(list, c.x, min.y + 154 * s, s, !(players && i == 0));
        }
    }
    if (CaseClose()) {
        open = false;
    }
    ImGui::End();
}

}  // namespace bw
