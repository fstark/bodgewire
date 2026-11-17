#include "ui/matrix_keyboard_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "parts/matrix_keyboard.hpp"
#include "parts/zx80_keys.hpp"
#include "ui/bench.hpp"
#include "view/matrix_keyboard_layout.hpp"

namespace bw {

namespace {

// Geometry in key pitches (u).
constexpr float kMargin = 0.55f;  // the bezel around the keys
constexpr float kLabel = 0.40f;   // the name strip above the keys
constexpr float kInset = 0.10f;   // the case inside the window
constexpr float kGap = 0.16f;     // between chiclets: the PET's keys stand apart
constexpr float kMinWidth = 380;

// The case in u, by the keyboard drawn.
float WidthU(KeyMatrixKind kind) {
    return (kind == KeyMatrixKind::Bw80 ? kZx80KeyboardWidth : kMatrixKeyboardWidth) + 2 * kMargin;
}
float HeightU(KeyMatrixKind kind) {
    return (kind == KeyMatrixKind::Bw80 ? kZx80KeyboardHeight : kMatrixKeyboardRows) + 2 * kMargin +
           kLabel;
}

// The ZX80's membrane (P5x): a black panel, blue keys, white letters and keywords, yellow shifted
// legends, a thin light line along the top.
constexpr ImU32 kPanel = IM_COL32(10, 10, 12, 255);
constexpr ImU32 kPanelLine = IM_COL32(200, 200, 204, 255);
constexpr ImU32 kMembraneTop = IM_COL32(28, 48, 226, 255);
constexpr ImU32 kMembraneBottom = IM_COL32(18, 32, 176, 255);
constexpr ImU32 kMembraneDownTop = IM_COL32(16, 26, 128, 255);
constexpr ImU32 kMembraneDownBottom = IM_COL32(12, 20, 96, 255);
constexpr ImU32 kMembraneEdge = IM_COL32(60, 80, 240, 255);
constexpr ImU32 kWhite = IM_COL32(244, 244, 246, 255);
constexpr ImU32 kYellow = IM_COL32(236, 216, 40, 255);

constexpr ImU32 kSurround = IM_COL32(8, 8, 9, 255);
constexpr ImU32 kCaseTop = IM_COL32(52, 56, 64, 255);  // the PET's blue-grey metal
constexpr ImU32 kCaseBottom = IM_COL32(30, 32, 37, 255);
constexpr ImU32 kCaseEdge = IM_COL32(90, 96, 106, 255);
constexpr ImU32 kPlate = IM_COL32(16, 17, 20, 255);  // the black plate the keys stand on
constexpr ImU32 kTypingEdge = IM_COL32(255, 176, 40, 255);
constexpr ImU32 kEngraving = IM_COL32(150, 156, 166, 255);
constexpr ImU32 kCapTop = IM_COL32(222, 218, 206, 255);  // off-white chiclet
constexpr ImU32 kCapBottom = IM_COL32(186, 181, 168, 255);
constexpr ImU32 kCapDownTop = IM_COL32(176, 171, 158, 255);
constexpr ImU32 kCapDownBottom = IM_COL32(160, 155, 143, 255);
constexpr ImU32 kCapSide = IM_COL32(96, 93, 86, 255);
constexpr ImU32 kCapShine = IM_COL32(255, 255, 255, 70);
constexpr ImU32 kLegend = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kShifted = IM_COL32(70, 70, 76, 255);

enum class Arrow : std::uint8_t { None, Left, Up, UpDown, LeftRight };

// The PET's arrows, drawn (the keycap font has no arrow glyphs): ← and ↑ alone, CRSR with ↕ or ↔.
Arrow ArrowOf(std::string_view legend) {
    if (legend == "_") {
        return Arrow::Left;
    }
    if (legend == "^") {
        return Arrow::Up;
    }
    if (legend == "CRSR DN") {
        return Arrow::UpDown;
    }
    if (legend == "CRSR RT") {
        return Arrow::LeftRight;
    }
    return Arrow::None;
}

// An arrow centred on `c`, `len` long, its heads `head` wide; `vertical` or horizontal; heads at
// the start, the end, or both.
void DrawArrow(ImDrawList& list, ImVec2 c, float len, float head, bool vertical, bool startHead,
               bool endHead, ImU32 colour = kLegend) {
    const float thick = std::max(1.0f, head * 0.28f);
    const ImVec2 a = vertical ? ImVec2{c.x, c.y - len / 2} : ImVec2{c.x - len / 2, c.y};
    const ImVec2 b = vertical ? ImVec2{c.x, c.y + len / 2} : ImVec2{c.x + len / 2, c.y};
    list.AddLine(a, b, colour, thick);
    const auto tip = [&](ImVec2 p, float dir) {  // dir: -1 the start, +1 the end
        if (vertical) {
            list.AddTriangleFilled(p, {p.x - head / 2, p.y - dir * head},
                                   {p.x + head / 2, p.y - dir * head}, colour);
        } else {
            list.AddTriangleFilled(p, {p.x - dir * head, p.y - head / 2},
                                   {p.x - dir * head, p.y + head / 2}, colour);
        }
    };
    if (startHead) {
        tip(a, -1);
    }
    if (endHead) {
        tip(b, 1);
    }
}

void DrawChiclet(ImDrawList& list, ImFont* font, std::string_view legend, std::string_view shifted,
                 ImVec2 a, ImVec2 b, float u, bool down) {
    const float rounding = 0.10f * u;
    const float depth = down ? 0.02f * u : 0.07f * u;
    // The side seen below the top, then the top.
    list.AddRectFilled({a.x, a.y + depth}, {b.x, b.y + 0.07f * u}, kCapSide, rounding);
    const ImVec2 ta{a.x, a.y + (down ? 0.05f * u : 0)};
    const ImVec2 tb{b.x, b.y - depth + (down ? 0.05f * u : 0)};
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(ta, tb, down ? kCapDownTop : kCapTop, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, ta,
                                                  {ta.x, tb.y}, down ? kCapDownTop : kCapTop,
                                                  down ? kCapDownBottom : kCapBottom);
    if (!down) {
        list.AddLine({ta.x + rounding, ta.y + 1}, {tb.x - rounding, ta.y + 1}, kCapShine);
    }
    const ImVec2 centre{(ta.x + tb.x) / 2, (ta.y + tb.y) / 2};
    const auto text = [&](std::string_view t, float size, float y, ImU32 colour) {
        const ImVec2 e = font->CalcTextSizeA(size, FLT_MAX, 0, t.data(), t.data() + t.size());
        list.AddText(font, size, {std::round(centre.x - e.x / 2), std::round(y - e.y / 2)}, colour,
                     t.data(), t.data() + t.size());
    };
    const float word = std::max(6.0f, std::round(0.20f * u));
    const float small = std::max(6.0f, std::round(0.17f * u));
    switch (ArrowOf(legend)) {
    case Arrow::Left:
        DrawArrow(list, centre, 0.42f * u, 0.18f * u, false, true, false);
        return;
    case Arrow::Up:
        DrawArrow(list, centre, 0.42f * u, 0.18f * u, true, true, false);
        return;
    case Arrow::UpDown:
        text("CRSR", word, centre.y - 0.18f * u, kLegend);
        DrawArrow(list, {centre.x, centre.y + 0.14f * u}, 0.30f * u, 0.12f * u, true, true, true);
        return;
    case Arrow::LeftRight:
        text("CRSR", word, centre.y - 0.18f * u, kLegend);
        DrawArrow(list, {centre.x, centre.y + 0.14f * u}, 0.40f * u, 0.12f * u, false, true, true);
        return;
    case Arrow::None:
        break;
    }
    const bool isWord = legend.size() > 3;
    if (!shifted.empty()) {
        text(shifted, small, ta.y + 0.17f * u, kShifted);
        text(legend, isWord ? word : std::max(6.0f, std::round(0.38f * u)), centre.y + 0.10f * u,
             kLegend);
        return;
    }
    text(legend, isWord ? word : std::max(6.0f, std::round(0.38f * u)), centre.y, kLegend);
}

}  // namespace

void MatrixKeyboardWindow::draw(bool& open, Session& session, MatrixCapture& capture) {
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
    const float aspect = HeightU(capture.kind) / WidthU(capture.kind);
    const Extent first = FirstSize(BenchObject::Keyboard, BenchArea().extent(), aspect);
    PlaceNext(BenchObject::Keyboard, {first.width, first.height});
    KeepAspect(aspect, kMinWidth);
    if (focusNext_) {
        ImGui::SetNextWindowFocus();
        focusNext_ = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          capture.kind == KeyMatrixKind::Bw80 ? kPanel : kSurround);
    const std::string title = "Keyboard " + capture.reference + kMatrixKeyboardWindowName;
    const bool visible =
        ImGui::Begin(title.c_str(), &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing);
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
    if (capture.kind == KeyMatrixKind::Bw80) {
        drawMembrane(session, capture);
    } else {
        drawKeys(session, capture);
    }
    if (CaseClose()) {
        open = false;
        stopTyping();
    }
    ImGui::End();
}

template <class Draw>
void MatrixKeyboardWindow::drawCaps(Session& session, const MatrixCapture& capture,
                                    std::span<const MatrixCap> caps, std::size_t shift, ImVec2 keys,
                                    float u, Draw&& draw) {
    latched_.resize(std::max(latched_.size(), caps.size()), false);
    std::vector<bool> down(latched_.size(), false);
    for (const MatrixPress& p : capture.held()) {
        if (p.key < down.size()) {
            down[p.key] = true;
        }
        if (p.shift && shift < down.size()) {
            down[shift] = true;
        }
    }
    for (const MatrixCap& cap : caps) {
        const ImVec2 a{keys.x + (cap.x + kGap / 2) * u, keys.y + (cap.y + kGap / 2) * u};
        const ImVec2 b{a.x + (cap.width - kGap) * u, a.y + (1 - kGap) * u};
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(static_cast<int>(cap.key));
        ImGui::InvisibleButton("key", {b.x - a.x, b.y - a.y},
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonRight);
        ImGui::PopID();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            latched_[cap.key] = !latched_[cap.key];
        }
        const bool mouse = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool held = mouse || latched_[cap.key];
        if (held != session.matrixKeyHeld(capture.reference, cap.key)) {
            session.setMatrixKeyHeld(capture.reference, cap.key, held);  // ignored when off
        }
        draw(cap, a, b, down[cap.key] || held || session.matrixKeyDown(capture.reference, cap.key));
    }
}

void MatrixKeyboardWindow::drawKeys(Session& session, const MatrixCapture& capture) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float widthU = WidthU(capture.kind), heightU = HeightU(capture.kind);
    const float u = std::max(1.0f, std::min(avail.x / widthU, avail.y / heightU));
    ImDrawList& list = *ImGui::GetWindowDrawList();

    const ImVec2 caseA{origin.x + kInset * u, origin.y + kInset * u};
    const ImVec2 caseB{origin.x + (widthU - kInset) * u, origin.y + (heightU - kInset) * u};
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(caseA, caseB, kCaseTop, 0.2f * u);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, caseA,
                                                  {caseA.x, caseB.y}, kCaseTop, kCaseBottom);
    list.AddLine({caseA.x + 0.2f * u, caseA.y + 0.5f}, {caseB.x - 0.2f * u, caseA.y + 0.5f},
                 kCaseEdge);
    if (capture.on) {
        list.AddRect(caseA, caseB, kTypingEdge, 0.2f * u, 0, 2.0f);
    }
    const ImVec2 keys{origin.x + kMargin * u, origin.y + (kMargin + kLabel) * u};
    const std::string engraving = "BW-2001 \xc2\xb7 " + capture.reference;
    list.AddText(legends_, std::max(6.0f, std::round(0.24f * u)),
                 {std::round(keys.x), std::round(origin.y + kMargin * u)}, kEngraving,
                 engraving.c_str());
    // The plates under the main block and the pad.
    const float lip = 0.12f * u;
    list.AddRectFilled({keys.x - lip, keys.y - lip},
                       {keys.x + 12 * u + lip, keys.y + kMatrixKeyboardRows * u + lip}, kPlate,
                       0.1f * u);
    list.AddRectFilled(
        {keys.x + kMatrixPadX * u - lip, keys.y - lip},
        {keys.x + (kMatrixPadX + 4) * u + lip, keys.y + kMatrixKeyboardRows * u + lip}, kPlate,
        0.1f * u);

    const auto all = Bw2001Keys();
    drawCaps(session, capture, Bw2001Caps(), *Bw2001KeyAt(8, 0), keys, u,
             [&](const MatrixCap& cap, ImVec2 a, ImVec2 b, bool down) {
                 const MatrixKey& key = all[cap.key];
                 DrawChiclet(list, legends_, key.legend, key.shifted, a, b, u, down);
             });
}

namespace {

// One ZX80 key: the blue cap with its letter at the lower left, its shifted legend at the upper
// right in yellow (a graphic as a small mosaic), and above the cap the keyword in white or, for
// the digits and NEW LINE, the shifted word or arrow in yellow.
void DrawMembraneKey(ImDrawList& list, ImFont* font, const Zx80Key& key, ImVec2 a, ImVec2 b,
                     float u, bool down) {
    const float rounding = 0.12f * u;
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(a, b, down ? kMembraneDownTop : kMembraneTop, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, a, {a.x, b.y},
                                                  down ? kMembraneDownTop : kMembraneTop,
                                                  down ? kMembraneDownBottom : kMembraneBottom);
    if (!down) {
        list.AddRect(a, b, kMembraneEdge, rounding, 0, 1.0f);
    }
    const auto size = [&](std::string_view s, float px) {
        return font->CalcTextSizeA(px, FLT_MAX, 0, s.data(), s.data() + s.size());
    };
    const auto text = [&](std::string_view s, float px, ImVec2 at, ImU32 colour) {
        list.AddText(font, px, {std::round(at.x), std::round(at.y)}, colour, s.data(),
                     s.data() + s.size());
    };
    const float big = std::max(6.0f, std::round(0.34f * u));
    const float word = std::max(6.0f, std::round(0.18f * u));
    const float small = std::max(6.0f, std::round(0.22f * u));
    const float centreX = (a.x + b.x) / 2;
    // The letter, digit or word on the cap.
    if (key.legend == "NEW LINE") {
        text("NEW", word, {centreX - size("NEW", word).x / 2, a.y + 0.22f * u}, kWhite);
        text("LINE", word, {centreX - size("LINE", word).x / 2, a.y + 0.50f * u}, kWhite);
    } else if (key.legend.size() > 1) {
        text(key.legend, word, {centreX - size(key.legend, word).x / 2, b.y - 0.17f * u - word},
             kWhite);
    } else {
        text(key.legend, big, {a.x + 0.14f * u, b.y - 0.12f * u - big}, kWhite);
    }
    // Above the cap: the keyword (white), or the digits' and NEW LINE's shifted word (yellow).
    // Words go above the cap (the digits' and NEW LINE's); symbols, OR, ** and the pound sign
    // stay on it.
    const bool isWord = key.shifted.size() > 2 && std::ranges::all_of(key.shifted, [](char c) {
                            return c >= 'A' && c <= 'Z';
                        });
    const bool shiftedAbove = isWord && key.shifted != "GRAPHIC";
    const bool arrow = key.shifted.size() == 3 && key.shifted[0] == '\xe2';
    if (!key.keyword.empty()) {
        text(key.keyword, word, {centreX - size(key.keyword, word).x / 2, a.y - 0.05f * u - word},
             kWhite);
    } else if (arrow) {
        const ImVec2 c{centreX, a.y - 0.05f * u - word / 2};
        const bool vertical = key.shifted == "\xe2\x86\x91" || key.shifted == "\xe2\x86\x93";
        const bool endHead = key.shifted == "\xe2\x86\x92" || key.shifted == "\xe2\x86\x93";
        DrawArrow(list, c, 0.36f * u, 0.14f * u, vertical, !endHead, endHead, kYellow);
    } else if (shiftedAbove) {
        text(key.shifted, word, {centreX - size(key.shifted, word).x / 2, a.y - 0.05f * u - word},
             kYellow);
    }
    // The upper right: a symbol, OR, **, pound, or a graphic's mosaic.
    if (key.shifted == "GRAPHIC") {
        const float s = 0.13f * u;
        const ImVec2 g{b.x - 0.12f * u - 2 * s, a.y + 0.12f * u};
        list.AddRectFilled(g, {g.x + 2 * s, g.y + 2 * s}, kYellow);
        list.AddRectFilled({g.x + s, g.y}, {g.x + 2 * s, g.y + s}, kMembraneTop);
        list.AddRectFilled({g.x, g.y + s}, {g.x + s, g.y + 2 * s}, kMembraneTop);
    } else if (!key.shifted.empty() && !shiftedAbove && !arrow) {
        text(key.shifted, small, {b.x - 0.12f * u - size(key.shifted, small).x, a.y + 0.08f * u},
             kYellow);
    }
}

}  // namespace

void MatrixKeyboardWindow::drawMembrane(Session& session, const MatrixCapture& capture) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float widthU = WidthU(capture.kind), heightU = HeightU(capture.kind);
    const float u = std::max(1.0f, std::min(avail.x / widthU, avail.y / heightU));
    ImDrawList& list = *ImGui::GetWindowDrawList();

    const ImVec2 panelA{origin.x + kInset * u, origin.y + kInset * u};
    const ImVec2 panelB{origin.x + (widthU - kInset) * u, origin.y + (heightU - kInset) * u};
    list.AddRectFilled(panelA, panelB, kPanel, 0.1f * u);
    list.AddLine({panelA.x + 0.3f * u, panelA.y + 0.12f * u},
                 {panelB.x - 0.3f * u, panelA.y + 0.12f * u}, kPanelLine,
                 std::max(1.0f, 0.02f * u));
    if (capture.on) {
        list.AddRect(panelA, panelB, kTypingEdge, 0.1f * u, 0, 2.0f);
    }
    const ImVec2 keys{origin.x + kMargin * u, origin.y + (kMargin + kLabel) * u};
    const std::string engraving = "BW-80 \xc2\xb7 " + capture.reference;
    list.AddText(legends_, std::max(6.0f, std::round(0.24f * u)),
                 {std::round(keys.x), std::round(origin.y + kMargin * u)}, kEngraving,
                 engraving.c_str());
    const auto all = Zx80Keys();
    drawCaps(session, capture, Zx80Caps(), *Zx80KeyAt(0, 0), keys, u,
             [&](const MatrixCap& cap, ImVec2 a, ImVec2 b, bool down) {
                 DrawMembraneKey(list, legends_, all[cap.key], a, b, u, down);
             });
}

}  // namespace bw
