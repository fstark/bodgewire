#include "ui/matrix_keyboard_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "parts/matrix_keyboard.hpp"
#include "parts/zx80_keys.hpp"
#include "ui/bench.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/pet_graphics.hpp"
#include "view/matrix_keyboard_layout.hpp"

namespace bw {

namespace {

// Geometry in key pitches (u).
constexpr float kMargin = 0.55f;  // the bezel around the keys
constexpr float kLabel = 0.40f;   // the name strip above the keys
constexpr float kInset = 0.10f;   // the case inside the window
constexpr float kGap = 0.06f;     // between the PET's keys: a grid, nearly touching
constexpr float kMinWidth = 380;

// The BW-80's case (P6l): the ZX80's cream lip around the black membrane, the membrane's black
// around its keys, the screws in its bottom corners, the reference moulded into the bottom lip.
constexpr float kLipSide = 0.30f;
constexpr float kLipTop = 0.45f;  // room for the put-away ×
constexpr float kLipBottom = 0.42f;
constexpr float kMembraneSide = 0.30f;
constexpr float kMembraneTop = 0.10f;  // above the digits' words
constexpr float kMembraneBottom = 0.45f;

// The case in u, by the keyboard drawn.
float WidthU(KeyMatrixKind kind) {
    return kind == KeyMatrixKind::Bw80 ? kZx80KeyboardWidth + 2 * (kLipSide + kMembraneSide)
                                       : kMatrixKeyboardWidth + 2 * kMargin;
}
float HeightU(KeyMatrixKind kind) {
    return kind == KeyMatrixKind::Bw80
               ? kZx80KeyboardHeight + kLipTop + kMembraneTop + kMembraneBottom + kLipBottom
               : kMatrixKeyboardRows + 2 * kMargin + kLabel;
}

// The ZX80's membrane (P5x, matched to a photograph in P6l): a soft black panel, mid-blue keys,
// pale letters, white keywords, muted gold shifted legends (paler on the keys), BREAK in dark blue
// on the black, the arrows as hollow block arrows outlined in gold.
constexpr ImU32 kPanel = IM_COL32(26, 24, 29, 255);
constexpr ImU32 kKeyTop = IM_COL32(38, 94, 214, 255);
constexpr ImU32 kKeyBottom = IM_COL32(22, 70, 184, 255);
constexpr ImU32 kKeyDownTop = IM_COL32(20, 54, 140, 255);
constexpr ImU32 kKeyDownBottom = IM_COL32(16, 44, 120, 255);
constexpr ImU32 kKeyLetter = IM_COL32(224, 234, 250, 255);
constexpr ImU32 kKeyword = IM_COL32(234, 234, 238, 255);
constexpr ImU32 kGold = IM_COL32(212, 180, 98, 255);        // on the black
constexpr ImU32 kGoldOnKey = IM_COL32(234, 210, 128, 255);  // on the blue
constexpr ImU32 kGraphicInk = IM_COL32(214, 176, 48, 255);
constexpr ImU32 kGraphicFrame = IM_COL32(228, 230, 236, 255);
constexpr ImU32 kBreak = IM_COL32(56, 70, 168, 255);
constexpr ImU32 kCream = IM_COL32(236, 230, 212, 255);
constexpr ImU32 kCreamShade = IM_COL32(214, 207, 186, 255);
constexpr ImU32 kCreamText = IM_COL32(150, 142, 122, 255);
constexpr ImU32 kScrew = IM_COL32(48, 46, 50, 255);
constexpr ImU32 kScrewShine = IM_COL32(96, 94, 100, 255);

constexpr ImU32 kCaseTop = IM_COL32(234, 234, 230, 255);  // the PET's white-painted metal
constexpr ImU32 kCaseBottom = IM_COL32(214, 214, 208, 255);
constexpr ImU32 kCaseEdge = IM_COL32(250, 250, 248, 255);
constexpr ImU32 kPlate = IM_COL32(16, 17, 20, 255);  // the black plate the keys stand on
constexpr ImU32 kEngraving = IM_COL32(120, 120, 116, 255);
// The PET 2001's point-of-sale keys (P6l2), from a photograph: a cream body, a rounded top set in
// it (grey on the main block, near-white on the pad, red or blue on the keys with
// words), black legends and graphics, white words on the coloured tops.
constexpr ImU32 kPetBody = IM_COL32(232, 226, 206, 255);
constexpr ImU32 kPetBodyShade = IM_COL32(196, 189, 168, 255);
constexpr ImU32 kPetRim = IM_COL32(248, 244, 230, 255);     // the rim's highlight
constexpr ImU32 kPetRecess = IM_COL32(120, 112, 100, 255);  // the recess's shadow
constexpr ImU32 kPetGrey = IM_COL32(164, 164, 170, 255);    // the silver foil
constexpr ImU32 kPetWhite = IM_COL32(240, 238, 230, 255);
constexpr ImU32 kPetRed = IM_COL32(186, 60, 54, 255);
constexpr ImU32 kPetBlue = IM_COL32(36, 158, 218, 255);
constexpr ImU32 kPetWord = IM_COL32(248, 244, 244, 255);
constexpr ImU32 kLegend = IM_COL32(22, 22, 24, 255);
constexpr ImU32 kPressed = IM_COL32(0, 0, 0, 48);

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

// A colour a fifth of the way to white: the light on the top of a foil label.
ImU32 Lighter(ImU32 c) {
    const auto mix = [&](int shift) {
        const int v = static_cast<int>((c >> shift) & 0xFF);
        return v + (255 - v) / 5;
    };
    return IM_COL32(mix(IM_COL32_R_SHIFT), mix(IM_COL32_G_SHIFT), mix(IM_COL32_B_SHIFT), 255);
}

// One PET key in its pitch `a`-`b`: the cream body, the coloured top, the graphic SHIFT gives above
// the legend (from the character ROM), or the words of the keys that have them. `pad`: a key of
// the numeric pad (near-white tops).
void DrawPetKey(ImDrawList& list, ImFont* font, std::string_view legend, ImVec2 a, ImVec2 b,
                float u, bool down, bool pad) {
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(a, b, kPetBody, 0.06f * u);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, a, {a.x, b.y},
                                                  kPetBody, kPetBodyShade);
    // The raised rim around the recess, then the label in it, centred: the key is symmetrical.
    const ImVec2 ra{a.x + 0.06f * u, a.y + 0.06f * u}, rb{b.x - 0.06f * u, b.y - 0.06f * u};
    const int rim = list.VtxBuffer.Size;
    list.AddRectFilled(ra, rb, kPetRim, 0.09f * u);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, rim, list.VtxBuffer.Size, ra, {ra.x, rb.y},
                                                  kPetRim, kPetBody);
    const ImVec2 ta{a.x + 0.14f * u, a.y + 0.14f * u}, tb{b.x - 0.14f * u, b.y - 0.14f * u};
    list.AddRectFilled({ta.x - 0.02f * u, ta.y - 0.02f * u}, tb, kPetRecess, 0.07f * u);
    ImU32 top = pad ? kPetWhite : kPetGrey;
    ImU32 ink = kLegend;
    if (legend == "SHIFT" || legend == "SPACE" || legend == "RETURN" || legend == "HOME" ||
        legend == "DEL") {
        top = kPetRed, ink = kPetWord;
    } else if (legend == "RVS" || legend == "STOP" || legend.starts_with("CRSR")) {
        top = kPetBlue, ink = kPetWord;
    }
    // The label's foil: lighter at the top, a shadow along its top and left edge from the rim.
    const ImVec2 la{ta.x + 0.012f * u, ta.y + 0.012f * u};
    const int foil = list.VtxBuffer.Size;
    list.AddRectFilled(la, tb, top, 0.06f * u);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, foil, list.VtxBuffer.Size, la,
                                                  {la.x, tb.y}, Lighter(top), top);
    if (down) {
        list.AddRectFilled(a, b, kPressed, 0.06f * u);
    }
    const float cx = (la.x + tb.x) / 2, cy = (la.y + tb.y) / 2;
    const float px = std::max(6.0f, std::round(0.28f * u));
    const float density = ImGui::GetWindowViewport()->FramebufferScale.x;
    const auto centred = [&](std::string_view t, float y, float size = 0) {
        const float sz = size > 0 ? size : px;
        const ImVec2 e = font->CalcTextSizeA(sz, FLT_MAX, 0, t.data(), t.data() + t.size());
        const ImVec2 at{std::round(cx - e.x / 2), std::round(y - e.y / 2)};
        // Struck twice a device pixel apart: the 2001's legends are bold, the keycap font is not.
        list.AddText(font, sz, at, ink, t.data(), t.data() + t.size());
        list.AddText(font, sz, {at.x + 1 / density, at.y}, ink, t.data(), t.data() + t.size());
    };
    const auto twoLines = [&](std::string_view upper, std::string_view lower) {
        const float sz = std::max(6.0f, std::round(0.19f * u));
        centred(upper, cy - 0.12f * u, sz);
        centred(lower, cy + 0.12f * u, sz);
    };
    if (legend == "RETURN") {  // two rows high, its letters stacked
        const float sz = std::max(6.0f, std::round(0.20f * u));
        constexpr std::string_view kWord = "RETURN";
        const float step = std::min(0.22f * u, (tb.y - ta.y) / 7);
        for (std::size_t i = 0; i < kWord.size(); ++i) {
            centred(kWord.substr(i, 1), cy + (static_cast<float>(i) - 2.5f) * step, sz);
        }
        return;
    }
    if (legend == "SHIFT" || legend == "SPACE") {
        centred(legend, cy, std::max(6.0f, std::round(0.19f * u)));
        return;
    }
    if (legend == "RVS" || legend == "STOP" || legend == "HOME" || legend == "DEL") {
        twoLines(legend == "RVS"    ? "OFF"
                 : legend == "STOP" ? "RUN"
                 : legend == "HOME" ? "CLR"
                                    : "INST",
                 legend);
        return;
    }
    if (legend.starts_with("CRSR")) {
        const bool vertical = legend == "CRSR DN";
        centred("CRSR", cy, std::max(6.0f, std::round(0.18f * u)));
        const float len = 0.24f * u, head = 0.09f * u;
        if (vertical) {
            DrawArrow(list, {cx, cy - 0.19f * u}, 0.12f * u, head, true, true, false, ink);
            DrawArrow(list, {cx, cy + 0.19f * u}, 0.12f * u, head, true, false, true, ink);
        } else {
            DrawArrow(list, {cx, cy - 0.17f * u}, len, head, false, true, false, ink);
            DrawArrow(list, {cx, cy + 0.17f * u}, len, head, false, false, true, ink);
        }
        return;
    }
    // A graphic above, the legend below: an arrow for ← and ↑, a slashed zero on the pad.
    if (const auto* glyph = PetGraphicOf(legend)) {
        // Each pixel whole device pixels (half points on a Retina display), so the check is even.
        const float cell = std::max(1.0f, std::floor(0.036f * u * density)) / density;
        const float gx = std::round(cx - 4 * cell), gy = std::round(cy - 0.05f * u - 8 * cell);
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                if (((*glyph)[y] >> (7 - x)) & 1) {
                    list.AddRectFilled({gx + x * cell, gy + y * cell},
                                       {gx + (x + 1) * cell, gy + (y + 1) * cell}, ink);
                }
            }
        }
    }
    const float ly = cy + 0.03f * u + px / 2;
    if (legend == "_") {
        DrawArrow(list, {cx, ly}, 0.26f * u, 0.11f * u, false, true, false, ink);
    } else if (legend == "^") {
        DrawArrow(list, {cx, ly + 0.02f * u}, 0.16f * u, 0.09f * u, true, true, false, ink);
    } else {
        centred(legend, ly);
        if (pad && legend == "0") {  // the 2001's slashed zero
            const ImVec2 e = font->CalcTextSizeA(px, FLT_MAX, 0, "0", nullptr);
            list.AddLine({cx + e.x * 0.35f, ly - e.y * 0.32f}, {cx - e.x * 0.35f, ly + e.y * 0.32f},
                         ink, std::max(1.0f, 0.025f * u));
        }
    }
}

}  // namespace

bool MatrixKeyboardWindow::draw(bool& open, Session& session, MatrixCapture& capture) {
    if (capture.reference.empty() || !open) {
        return false;
    }
    const float aspect = HeightU(capture.kind) / WidthU(capture.kind);
    const Extent first = FirstSize(BenchObject::Keyboard, BenchArea().extent(), aspect);
    PlaceNext(BenchObject::Keyboard, {first.width, first.height});
    KeepAspect(aspect, kMinWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          capture.kind == KeyMatrixKind::Bw80 ? kCream : kCaseTop);
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
        return false;
    }
    const bool clicked = InstrumentClicked();
    SharpFont(legends_);
    if (capture.kind == KeyMatrixKind::Bw80) {
        drawMembrane(session, capture);
    } else {
        drawKeys(session, capture);
    }
    if (CaseClose(capture.kind == KeyMatrixKind::Bw80 ? 3 : 6)) {
        open = false;
    }
    ImGui::End();
    return clicked;
}

template <class Draw>
void MatrixKeyboardWindow::drawCaps(Session& session, const MatrixCapture& capture,
                                    std::span<const MatrixCap> caps, std::size_t shift, ImVec2 keys,
                                    float u, ImVec2 gap, float height, Draw&& draw) {
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
        const ImVec2 a{keys.x + (cap.x + gap.x / 2) * u, keys.y + (cap.y + gap.y / 2) * u};
        const ImVec2 b{a.x + (cap.width - gap.x) * u, a.y + (cap.height * height - gap.y) * u};
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
        list.AddRect(caseA, caseB, kFocusGold, 0.2f * u, 0, 2.0f);
    }
    const ImVec2 keys{origin.x + kMargin * u, origin.y + (kMargin + kLabel) * u};
    const std::string engraving = "BW-2001 \xc2\xb7 " + capture.reference;
    list.AddText(legends_, std::max(6.0f, std::round(0.24f * u)),
                 {std::round(keys.x), std::round(origin.y + kMargin * u)}, kEngraving,
                 engraving.c_str());
    // The plates under the main block and the pad.
    const float lip = 0.12f * u;
    list.AddRectFilled(
        {keys.x - lip, keys.y - lip},
        {keys.x + kMatrixMainWidth * u + lip, keys.y + kMatrixKeyboardRows * u + lip}, kPlate,
        0.1f * u);
    list.AddRectFilled(
        {keys.x + kMatrixPadX * u - lip, keys.y - lip},
        {keys.x + (kMatrixPadX + 4) * u + lip, keys.y + kMatrixKeyboardRows * u + lip}, kPlate,
        0.1f * u);

    const auto all = Bw2001Keys();
    drawCaps(session, capture, Bw2001Caps(), *Bw2001KeyAt(8, 0), keys, u, {kGap, kGap}, 1,
             [&](const MatrixCap& cap, ImVec2 a, ImVec2 b, bool down) {
                 DrawPetKey(list, legends_, all[cap.key].legend, a, b, u, down,
                            cap.x >= kMatrixPadX);
             });
}

namespace {

// A ZX80 graphic as the ROM's character set draws it (8 rows of 8 pixels, bit 7 the left; IC2 at
// 0x0E00): the code SHIFT and the key type (the key table at 0x0093). The greys are a one-pixel
// check (P6l: checked against zx80_ic2.bin).
using Glyph = std::array<std::uint8_t, 8>;

Glyph GraphicOf(std::string_view legend) {
    constexpr Glyph kLeft{0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0};      // Q: code 2
    constexpr Glyph kBottom{0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};                // W: 3
    constexpr Glyph kTopLeft{0xF0, 0xF0, 0xF0, 0xF0, 0, 0, 0, 0};               // E: 4
    constexpr Glyph kTopRight{0x0F, 0x0F, 0x0F, 0x0F, 0, 0, 0, 0};              // R: 5
    constexpr Glyph kBottomLeft{0, 0, 0, 0, 0xF0, 0xF0, 0xF0, 0xF0};            // D: 6
    constexpr Glyph kBottomRight{0, 0, 0, 0, 0x0F, 0x0F, 0x0F, 0x0F};           // F: 7
    constexpr Glyph kDiagonal{0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0};  // S: 8
    constexpr Glyph kGrey{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};      // A: 9
    constexpr Glyph kGreyBottom{0, 0, 0, 0, 0xAA, 0x55, 0xAA, 0x55};            // T: 10
    constexpr Glyph kGreyTop{0xAA, 0x55, 0xAA, 0x55, 0, 0, 0, 0};               // G: 11
    if (legend == "Q")
        return kLeft;
    if (legend == "W")
        return kBottom;
    if (legend == "E")
        return kTopLeft;
    if (legend == "R")
        return kTopRight;
    if (legend == "T")
        return kGreyBottom;
    if (legend == "A")
        return kGrey;
    if (legend == "S")
        return kDiagonal;
    if (legend == "D")
        return kBottomLeft;
    if (legend == "F")
        return kBottomRight;
    if (legend == "G")
        return kGreyTop;
    return {};
}

// A graphic about `s` square at `at`: a pale frame around its 8 by 8 pixels, gold ink on the
// key's blue, each pixel a whole number of screen points so the check stays even.
void DrawGraphic(ImDrawList& list, std::string_view legend, ImVec2 at, float s) {
    const float frame = std::max(1.0f, std::round(s * 0.07f));
    const float cell = std::max(1.0f, std::floor((s - 2 * frame) / 8));
    const float x0 = std::round(at.x + frame), y0 = std::round(at.y + frame);
    list.AddRectFilled({x0 - frame, y0 - frame}, {x0 + 8 * cell + frame, y0 + 8 * cell + frame},
                       kGraphicFrame);
    list.AddRectFilled({x0, y0}, {x0 + 8 * cell, y0 + 8 * cell}, kKeyTop);
    const Glyph glyph = GraphicOf(legend);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if ((glyph[y] >> (7 - x)) & 1) {
                list.AddRectFilled({x0 + x * cell, y0 + y * cell},
                                   {x0 + (x + 1) * cell, y0 + (y + 1) * cell}, kGraphicInk);
            }
        }
    }
}

// A hollow block arrow outlined in gold, as printed above 5-8: centred on `c`, `w` wide and `h`
// high (its head's span), pointing left, right, up or down.
void DrawBlockArrow(ImDrawList& list, ImVec2 c, float w, float h, char dir, float thick) {
    // Pointing right in a unit box (x 0..1 along the arrow, y -0.5..0.5 across it).
    constexpr std::array<ImVec2, 7> kShape{{{0.0f, -0.22f},
                                            {0.52f, -0.22f},
                                            {0.52f, -0.5f},
                                            {1.0f, 0.0f},
                                            {0.52f, 0.5f},
                                            {0.52f, 0.22f},
                                            {0.0f, 0.22f}}};
    std::array<ImVec2, 7> points{};
    const bool vertical = dir == 'u' || dir == 'd';
    const float along = vertical ? h : w, across = vertical ? w : h;
    const float sign = (dir == 'r' || dir == 'd') ? 1.0f : -1.0f;
    for (std::size_t i = 0; i < kShape.size(); ++i) {
        const float x = sign * (kShape[i].x - 0.5f) * along;
        const float y = kShape[i].y * across;
        points[i] = vertical ? ImVec2{c.x + y, c.y + x} : ImVec2{c.x + x, c.y + y};
    }
    list.AddPolyline(points.data(), static_cast<int>(points.size()), kGold, ImDrawFlags_Closed,
                     thick);
}

char ArrowDirection(std::string_view shifted) {
    if (shifted == "\xe2\x86\x90")
        return 'l';
    if (shifted == "\xe2\x86\x92")
        return 'r';
    if (shifted == "\xe2\x86\x91")
        return 'u';
    if (shifted == "\xe2\x86\x93")
        return 'd';
    return 0;
}

// One ZX80 key: the blue cap; its letter at the lower left (a digit centred), its shifted legend
// at the upper right (a graphic as its mosaic); above the cap its keyword in white or, for the
// digits and NEW LINE, its shifted word in gold or its arrow; SPACE's BREAK in dark blue.
void DrawMembraneKey(ImDrawList& list, ImFont* font, const Zx80Key& key, ImVec2 a, ImVec2 b,
                     float u, bool down) {
    const float rounding = 0.11f * u;
    const ImU32 top = down ? kKeyDownTop : kKeyTop, bottom = down ? kKeyDownBottom : kKeyBottom;
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(a, b, top, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, a, {a.x, b.y},
                                                  top, bottom);
    const auto size = [&](std::string_view s, float px) {
        return font->CalcTextSizeA(px, FLT_MAX, 0, s.data(), s.data() + s.size());
    };
    const auto text = [&](std::string_view s, float px, ImVec2 at, ImU32 colour) {
        list.AddText(font, px, {std::round(at.x), std::round(at.y)}, colour, s.data(),
                     s.data() + s.size());
    };
    const auto centred = [&](std::string_view s, float px, float y, ImU32 colour) {
        text(s, px, {(a.x + b.x) / 2 - size(s, px).x / 2, y}, colour);
    };
    const float big = std::max(7.0f, std::round(0.32f * u));
    const float word = std::max(6.0f, std::round(0.22f * u));
    const float onKey = std::max(6.0f, std::round(0.19f * u));
    const float symbol = std::max(6.0f, std::round(0.30f * u));
    const float midY = (a.y + b.y) / 2;
    const bool digit = key.legend.size() == 1 && key.legend[0] >= '0' && key.legend[0] <= '9';
    // On the cap.
    if (key.legend == "NEW LINE") {
        centred("NEW", onKey, midY - onKey * 0.95f, kKeyLetter);
        centred("LINE", onKey, midY - onKey * 0.05f, kKeyLetter);
    } else if (key.legend == "SHIFT") {
        centred(key.legend, onKey, midY - onKey / 2, kGoldOnKey);
    } else if (key.legend == "SPACE") {
        centred(key.legend, onKey, b.y - 0.10f * u - onKey, kKeyLetter);
    } else if (digit) {
        centred(key.legend, big, midY - big / 2, kKeyLetter);
    } else {
        text(key.legend, big, {a.x + 0.13f * u, b.y - 0.07f * u - big}, kKeyLetter);
    }
    // Above the cap: the keyword (white, BREAK dark blue), the digits' and NEW LINE's shifted
    // word in gold, or an arrow. OR, ** and the symbols stay on the cap.
    const bool shiftedWord =
        key.shifted.size() >= 2 && key.shifted != "OR" && key.shifted != "GRAPHIC" &&
        std::ranges::all_of(key.shifted, [](char c) { return c >= 'A' && c <= 'Z'; });
    const char arrow = ArrowDirection(key.shifted);
    const float aboveY = a.y - 0.07f * u - word;
    if (!key.keyword.empty()) {
        centred(key.keyword, word, aboveY, key.legend == "SPACE" ? kBreak : kKeyword);
    } else if (shiftedWord) {
        centred(key.shifted, word, aboveY, kGold);
    } else if (arrow != 0) {
        const bool vertical = arrow == 'u' || arrow == 'd';
        DrawBlockArrow(list, {(a.x + b.x) / 2, a.y - 0.19f * u}, vertical ? 0.46f * u : 0.50f * u,
                       vertical ? 0.24f * u : 0.25f * u, arrow, std::max(1.0f, 0.022f * u));
    }
    // The upper right: a symbol, OR, **, pound, or a graphic's mosaic.
    if (key.shifted == "GRAPHIC") {
        const float s = 0.27f * u;
        DrawGraphic(list, key.legend,
                    {std::round(b.x - 0.10f * u - s), std::round(a.y + 0.08f * u)}, s);
    } else if (!key.shifted.empty() && !shiftedWord && arrow == 0) {
        const float px = key.shifted == "OR" || key.shifted == "\xc2\xa3" ? 0.22f * u : symbol;
        text(key.shifted, px, {b.x - 0.11f * u - size(key.shifted, px).x, a.y + 0.05f * u},
             kGoldOnKey);
    }
}

// A screw head in the membrane's corner.
void DrawScrew(ImDrawList& list, ImVec2 c, float r) {
    list.AddCircleFilled(c, r, kScrew);
    list.AddCircle(c, r * 0.8f, kScrewShine, 0, std::max(1.0f, r * 0.12f));
    list.AddCircleFilled({c.x - r * 0.25f, c.y - r * 0.25f}, r * 0.25f, kScrewShine);
}

}  // namespace

void MatrixKeyboardWindow::drawMembrane(Session& session, const MatrixCapture& capture) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float widthU = WidthU(capture.kind), heightU = HeightU(capture.kind);
    const float u = std::max(1.0f, std::min(avail.x / widthU, avail.y / heightU));
    ImDrawList& list = *ImGui::GetWindowDrawList();

    // The cream lip, shaded a little towards the bottom.
    const ImVec2 caseA = origin;
    const ImVec2 caseB{origin.x + widthU * u, origin.y + heightU * u};
    const int first = list.VtxBuffer.Size;
    list.AddRectFilled(caseA, caseB, kCream, 0.12f * u);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(&list, first, list.VtxBuffer.Size, caseA,
                                                  {caseA.x, caseB.y}, kCream, kCreamShade);
    if (capture.on) {
        list.AddRect(caseA, caseB, kFocusGold, 0.12f * u, 0, 2.0f);
    }
    // The membrane, a shadow line along its top where it goes under the case.
    const ImVec2 panelA{origin.x + kLipSide * u, origin.y + kLipTop * u};
    const ImVec2 panelB{caseB.x - kLipSide * u, caseB.y - kLipBottom * u};
    list.AddRectFilled({panelA.x - 1, panelA.y - 1}, {panelB.x + 1, panelB.y + 1}, kCreamShade,
                       0.04f * u);
    list.AddRectFilled(panelA, panelB, kPanel, 0.04f * u);
    const float r = 0.13f * u;
    DrawScrew(list, {panelA.x + 0.22f * u, panelB.y - 0.22f * u}, r);
    DrawScrew(list, {panelB.x - 0.22f * u, panelB.y - 0.22f * u}, r);
    // The reference moulded into the bottom lip.
    const std::string engraving = "BW-80 \xc2\xb7 " + capture.reference;
    const float px = std::max(6.0f, std::round(0.20f * u));
    list.AddText(
        legends_, px,
        {std::round(panelA.x + 0.1f * u), std::round(panelB.y + (kLipBottom * u - px) / 2)},
        kCreamText, engraving.c_str());

    const ImVec2 keys{panelA.x + kMembraneSide * u, panelA.y + kMembraneTop * u};
    const auto all = Zx80Keys();
    drawCaps(session, capture, Zx80Caps(), *Zx80KeyAt(0, 0), keys, u, {1 - kZx80CapWidth, 0},
             kZx80CapHeight, [&](const MatrixCap& cap, ImVec2 a, ImVec2 b, bool down) {
                 DrawMembraneKey(list, legends_, all[cap.key], a, b, u, down);
             });
}

}  // namespace bw
