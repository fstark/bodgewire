#include "ui/keyboard_window.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "ui/toolbar.hpp"

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
constexpr float kInitialWidth = 720;  // points
constexpr float kMinWidth = 360;
constexpr auto kFlash = 150ms;

constexpr ImU32 kSurround = IM_COL32(8, 8, 9, 255);
constexpr ImU32 kCaseTop = IM_COL32(46, 46, 49, 255);
constexpr ImU32 kCaseBottom = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kCaseEdge = IM_COL32(78, 78, 82, 255);
constexpr ImU32 kWell = IM_COL32(9, 9, 10, 255);  // the opening the keys sit in
constexpr ImU32 kWellLip = IM_COL32(0, 0, 0, 120);
constexpr ImU32 kTypingEdge = IM_COL32(255, 176, 40, 255);
constexpr ImU32 kEngraving = IM_COL32(128, 128, 132, 255);
constexpr ImU32 kSkirt = IM_COL32(28, 28, 31, 255);
constexpr ImU32 kSkirtSpecial = IM_COL32(52, 53, 56, 255);
constexpr ImU32 kFaceTop = IM_COL32(58, 58, 62, 255);
constexpr ImU32 kFaceBottom = IM_COL32(40, 40, 43, 255);
constexpr ImU32 kSpecialTop = IM_COL32(92, 93, 97, 255);
constexpr ImU32 kSpecialBottom = IM_COL32(70, 71, 75, 255);
constexpr ImU32 kHighlight = IM_COL32(255, 255, 255, 28);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 30);  // each of three layers
constexpr ImU32 kLegend = IM_COL32(240, 236, 226, 255);

ImU32 Darker(ImU32 colour, float factor) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(colour);
    return ImGui::ColorConvertFloat4ToU32({c.x * factor, c.y * factor, c.z * factor, c.w});
}

bool IsLetter(std::string_view label) {
    return label.size() == 1 && label[0] >= 'A' && label[0] <= 'Z';
}

// The text `text` at `size` points, centred horizontally on `centreX`, its middle at `middleY`.
void Centred(ImDrawList& list, ImFont* font, float size, float centreX, float middleY,
             std::string_view text) {
    const ImVec2 extent =
        font->CalcTextSizeA(size, FLT_MAX, 0, text.data(), text.data() + text.size());
    list.AddText(font, size,
                 {std::round(centreX - extent.x / 2), std::round(middleY - extent.y / 2)}, kLegend,
                 text.data(), text.data() + text.size());
}

void DrawLegends(ImDrawList& list, ImFont* font, const KeyCap& cap, ImVec2 a, ImVec2 b, float u) {
    const float centre = (a.x + b.x) / 2;
    const float height = b.y - a.y;
    const auto points = [u](float share) { return std::max(6.0f, std::round(share * u)); };
    if (cap.kind != CapKind::Character) {
        // Control keys: small names, left-aligned at the bottom, BACK SPACE on two lines.
        const float size = points(0.20f);
        const std::string_view label = cap.label;
        const auto space = label.find(' ');
        std::vector<std::string_view> lines{label};
        if (space != std::string_view::npos) {
            lines = {label.substr(0, space), label.substr(space + 1)};
        }
        float y = b.y - 0.08f * u - size * static_cast<float>(lines.size());
        for (const std::string_view line : lines) {
            list.AddText(font, size, {std::round(a.x + 0.12f * u), std::round(y)}, kLegend,
                         line.data(), line.data() + line.size());
            y += size;
        }
        return;
    }
    if (cap.label.empty()) {
        return;  // the space bar
    }
    if (cap.shifted.empty()) {
        Centred(list, font, points(0.38f), centre, a.y + height * 0.5f, cap.label);
    } else if (IsLetter(cap.label)) {
        Centred(list, font, points(0.24f), centre, a.y + height * 0.20f, cap.shifted);
        Centred(list, font, points(0.38f), centre, a.y + height * 0.66f, cap.label);
    } else {
        Centred(list, font, points(0.30f), centre, a.y + height * 0.29f, cap.shifted);
        Centred(list, font, points(0.30f), centre, a.y + height * 0.71f, cap.label);
    }
}

// One cap in [a, b] (screen points), raised or pressed down.
void DrawCap(ImDrawList& list, ImFont* font, const KeyCap& cap, ImVec2 a, ImVec2 b, float u,
             bool down) {
    const bool special = cap.kind != CapKind::Character;
    const float rounding = 0.12f * u;
    if (down) {
        // The cap sinks into the keyboard: its top drops, its bottom edge stays hidden below.
        a.y += 0.12f * (b.y - a.y);
    } else {
        for (int i = 1; i <= 3; ++i) {
            const float grow = static_cast<float>(i);
            const float drop = 0.04f * u;
            list.AddRectFilled({a.x - grow, a.y + drop - grow + 2}, {b.x + grow, b.y + drop + grow},
                               kShadow, rounding + grow);
        }
    }
    list.AddRectFilled(a, b, special ? kSkirtSpecial : kSkirt, rounding);

    // The dished face, seen slightly from the front: inset more at the bottom.
    const ImVec2 faceA{a.x + 0.10f * u, a.y + 0.05f * u};
    const ImVec2 faceB{b.x - 0.10f * u, b.y - (down ? 0.10f : 0.17f) * u};
    const float faceRounding = 0.10f * u;
    const float shade = down ? 0.72f : 1.0f;
    const ImU32 top = Darker(special ? kSpecialTop : kFaceTop, shade);
    const ImU32 bottom = Darker(special ? kSpecialBottom : kFaceBottom, shade);
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(faceA, faceB, top, faceRounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, faceA,
                                                  {faceA.x, faceB.y}, top, bottom);
    list.AddLine({faceA.x + faceRounding, faceA.y + 0.5f}, {faceB.x - faceRounding, faceA.y + 0.5f},
                 kHighlight);
    DrawLegends(list, font, cap, faceA, faceB, u);
}

}  // namespace

void KeyboardWindow::flash(std::uint8_t code) {
    flash_ = CapFor(code);
    flashUntil_ = std::chrono::steady_clock::now() + kFlash;
}

void KeyboardWindow::draw(bool& open, Session& session, KeyboardCapture& capture) {
    if (capture.reference.empty()) {
        return;
    }
    if (!capture.on && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        open = true;
        focusNext_ = true;
    }
    const auto stopTyping = [&capture] {
        capture.on = false;
        capture.stop = false;
    };
    if (!open) {
        stopTyping();
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    titleHeight_ = ImGui::GetFrameHeight();
    const float aspect = kHeightU / kWidthU;
    const ImVec2 size{kInitialWidth, kInitialWidth * aspect + titleHeight_};
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + viewport->WorkSize.x * kBoardShare / 2 - size.x / 2,
         viewport->WorkPos.y + viewport->WorkSize.y - size.y - 16},
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        {kMinWidth, 0}, {FLT_MAX, FLT_MAX},
        [](ImGuiSizeCallbackData* data) {
            const float title = *static_cast<const float*>(data->UserData);
            data->DesiredSize.y = data->DesiredSize.x * kHeightU / kWidthU + title;
        },
        &titleHeight_);
    if (focusNext_) {
        ImGui::SetNextWindowFocus();
        focusNext_ = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kSurround);
    const bool visible =
        ImGui::Begin("Keyboard", &open,
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!visible || !open) {
        ImGui::End();
        stopTyping();
        return;
    }
    if (capture.stop) {
        ImGui::SetWindowFocus(nullptr);
        stopTyping();
    } else {
        capture.on = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }
    drawCaps(session, capture);
    ImGui::End();
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
        list.AddRect(caseA, caseB, kTypingEdge, caseRounding, 0, 2.0f);
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
        DrawCap(list, legends_, caps[i], rects[i].first, rects[i].second, u, isDown);
    }
}

}  // namespace bw
