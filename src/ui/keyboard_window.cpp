#include "ui/keyboard_window.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "ui/bench.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/keycap.hpp"

namespace bw {

namespace {

using namespace std::chrono_literals;

// Geometry, in key-widths (u): docs/p4o-keyboard-window/KEYBOARD_WINDOW_DESIGN.md §4.2.
constexpr float kCaseMargin = 0.5f;   // around the keys
constexpr float kLabelStrip = 0.45f;  // above row 0
constexpr float kCaseInset = 0.12f;   // the case inside the window
constexpr float kGap = 0.08f;         // between caps
constexpr float kWidthU = kKeyboardWidth + 2 * kCaseMargin;
constexpr float kHeightU = kKeyboardRows + 2 * kCaseMargin + kLabelStrip;
constexpr float kMinWidth = 360;
constexpr auto kFlash = 150ms;

constexpr ImU32 kSurround = IM_COL32(8, 8, 9, 255);
constexpr ImU32 kCaseTop = IM_COL32(46, 46, 49, 255);
constexpr ImU32 kCaseBottom = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kCaseEdge = IM_COL32(78, 78, 82, 255);
constexpr ImU32 kWell = IM_COL32(9, 9, 10, 255);  // the opening the keys sit in
constexpr ImU32 kWellLip = IM_COL32(0, 0, 0, 120);
constexpr ImU32 kEngraving = IM_COL32(128, 128, 132, 255);

}  // namespace

void KeyboardWindow::flash(std::uint8_t code) {
    flash_ = CapFor(code);
    flashUntil_ = std::chrono::steady_clock::now() + kFlash;
}

bool KeyboardWindow::draw(bool& open, Session& session, KeyboardCapture& capture) {
    if (capture.reference.empty() || !open) {
        return false;
    }

    const float aspect = kHeightU / kWidthU;
    const Extent first = FirstSize(BenchObject::Keyboard, BenchArea().extent(), aspect);
    PlaceNext(BenchObject::Keyboard, {first.width, first.height});
    KeepAspect(aspect, kMinWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kSurround);
    const bool visible =
        ImGui::Begin(kKeyboardWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!visible || !open) {
        ImGui::End();
        return false;
    }
    const bool clicked = InstrumentClicked();
    drawCaps(session, capture);
    if (CaseClose()) {
        open = false;
    }
    ImGui::End();
    return clicked;
}

void KeyboardWindow::drawCaps(Session& session, const KeyboardCapture& capture) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float u = std::max(1.0f, std::min(avail.x / kWidthU, avail.y / kHeightU));
    ImDrawList& list = *ImGui::GetWindowDrawList();

    // The case.
    const ImVec2 caseA{origin.x + kCaseInset * u, origin.y + kCaseInset * u};
    const ImVec2 caseB{origin.x + (kWidthU - kCaseInset) * u,
                       origin.y + (kHeightU - kCaseInset) * u};
    const float caseRounding = 0.25f * u;
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(caseA, caseB, kCaseTop, caseRounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, caseA,
                                                  {caseA.x, caseB.y}, kCaseTop, kCaseBottom);
    list.AddLine({caseA.x + caseRounding, caseA.y + 0.5f}, {caseB.x - caseRounding, caseA.y + 0.5f},
                 kCaseEdge);
    if (capture.on) {
        list.AddRect(caseA, caseB, kFocusGold, caseRounding, 0, 2.0f);
    }
    const ImVec2 keys{origin.x + kCaseMargin * u, origin.y + (kCaseMargin + kLabelStrip) * u};
    // The well: a dark opening a little larger than the keys, its top lip in shadow.
    const float lip = 0.10f * u;
    const ImVec2 wellA{keys.x - lip, keys.y - lip};
    const ImVec2 wellB{keys.x + kKeyboardWidth * u + lip, keys.y + kKeyboardRows * u + lip};
    list.AddRectFilled(wellA, wellB, kWell, 0.12f * u);
    list.AddRectFilled(wellA, {wellB.x, wellA.y + 0.06f * u}, kWellLip, 0.12f * u,
                       ImDrawFlags_RoundCornersTop);
    const std::string engraving = "ASCII KEYBOARD · " + capture.reference;
    list.AddText(legends_, std::max(6.0f, std::round(0.22f * u)),
                 {std::round(keys.x + 0.1f * u), std::round(origin.y + kCaseMargin * u)},
                 kEngraving, engraving.c_str());

    // What is down: PC keys held, the flash, the latches, the PC modifiers.
    const std::span<const KeyCap> caps = KeyCaps();
    std::vector<bool> down(caps.size(), false);
    bool shiftDown = shiftLatched_ || capture.shiftHeld();
    bool ctrlDown = ctrlLatched_ || capture.ctrlHeld();
    const auto press = [&](const CapPress& p) {
        down[p.cap] = true;
        shiftDown |= p.shift;
        ctrlDown |= p.ctrl;
    };
    for (const CapPress& p : capture.held()) {
        press(p);
    }
    if (flash_ && std::chrono::steady_clock::now() < flashUntil_) {
        press(*flash_);
    }

    // Buttons first (the mouse adds to what is down), then the drawing.
    std::vector<std::pair<ImVec2, ImVec2>> rects(caps.size());
    bool released = false;
    for (std::size_t i = 0; i < caps.size(); ++i) {
        const KeyCap& cap = caps[i];
        const ImVec2 a{keys.x + (cap.x + kGap / 2) * u, keys.y + (cap.y + kGap / 2) * u};
        const ImVec2 b{a.x + (cap.width - kGap) * u, a.y + (1 - kGap) * u};
        rects[i] = {a, b};
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("cap", {b.x - a.x, b.y - a.y});
        ImGui::PopID();
        if (ImGui::IsItemActivated()) {
            if (cap.kind == CapKind::Shift) {
                shiftLatched_ = !shiftLatched_;
            } else if (cap.kind == CapKind::Ctrl) {
                ctrlLatched_ = !ctrlLatched_;
            } else if (const auto code = CapCode(cap, shiftLatched_, ctrlLatched_)) {
                session.typeCode(capture.reference, *code);  // refused with the power off
            }
        }
        if (ImGui::IsItemActive() && cap.kind != CapKind::Shift && cap.kind != CapKind::Ctrl) {
            down[i] = true;
        }
        if (ImGui::IsItemDeactivated() && cap.kind != CapKind::Shift && cap.kind != CapKind::Ctrl) {
            released = true;
        }
    }
    shiftDown |= shiftLatched_;
    ctrlDown |= ctrlLatched_;
    if (released) {
        shiftLatched_ = ctrlLatched_ = false;  // drawn down until the key they modified is up
    }
    for (std::size_t i = 0; i < caps.size(); ++i) {
        const bool isDown = down[i] || (caps[i].kind == CapKind::Shift && shiftDown) ||
                            (caps[i].kind == CapKind::Ctrl && ctrlDown);
        DrawKeyCap(list, legends_, caps[i], rects[i].first, rects[i].second, u, isDown);
    }
}

}  // namespace bw
