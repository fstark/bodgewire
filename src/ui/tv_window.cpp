#include "ui/tv_window.hpp"

#include <cfloat>
#include <cmath>
#include <numbers>
#include <optional>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/gestures.hpp"
#include "ui/instrument_marks.hpp"
#include "view/bench_places.hpp"

namespace bw {

namespace {

constexpr float kMinWidth = 380;  // labels at least 9 px
constexpr float kMinHit = 44;     // px: a control's item, whatever the case's size (P8f)
constexpr float kSweep = 1.5f * std::numbers::pi_v<float>;   // 270°
constexpr float kStart = 0.75f * std::numbers::pi_v<float>;  // 0: lower left (screen angles)

// Fractions of the case's width, measured on a photo of the real set.
constexpr float kShellInset = 0.025f;
constexpr float kScreenLeft = 0.105f, kScreenTop = 0.13f, kScreenWidth = 0.79f;
constexpr float kScreenRounding = 0.045f;
constexpr float kBezelGrow = 0.018f;
constexpr float kStripLeft = 0.09f, kStripRight = 0.91f, kStripTop = 0.76f, kStripBottom = 0.925f;
constexpr float kLabelY = 0.79f, kKnobY = 0.86f, kFont = 0.024f;
constexpr float kKnobX[] = {0.17f, 0.285f, 0.40f, 0.515f};
constexpr float kSkirt = 0.026f, kCap = 0.019f, kKnobHit = 0.032f;
constexpr float kLampX = 0.60f, kLamp = 0.008f;
constexpr float kSlotLeft = 0.70f, kSlotRight = 0.82f, kSlotTop = 0.845f, kSlotBottom = 0.875f;
constexpr float kPowerY = 0.80f;
constexpr float kTrimmerX = 0.87f, kTrimmerY = 0.945f, kTrimmer = 0.009f, kTrimmerHit = 0.014f;

constexpr float kLoupeSource = 16;  // px of the picture across the loupe
constexpr float kLoupeZoom = 6;     // its magnification: 96 px across
constexpr ImU32 kLoupeRim = IM_COL32(20, 20, 22, 255);
constexpr ImU32 kLoupeGlint = IM_COL32(255, 255, 255, 60);
constexpr ImU32 kShell = IM_COL32(226, 220, 200, 255);
constexpr ImU32 kShellEdge = IM_COL32(170, 164, 148, 255);
constexpr ImU32 kFront = IM_COL32(48, 49, 52, 255);
constexpr ImU32 kFrontEdge = IM_COL32(78, 79, 84, 255);
constexpr ImU32 kBezel = IM_COL32(30, 30, 33, 255);
constexpr ImU32 kGlass = IM_COL32(36, 40, 38, 255);
constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kSheen = IM_COL32(255, 255, 255, 18);
constexpr ImU32 kStrip = IM_COL32(24, 24, 26, 255);
constexpr ImU32 kStripFrame = IM_COL32(165, 165, 168, 255);
constexpr ImU32 kLabel = IM_COL32(210, 210, 210, 255);
constexpr ImU32 kSkirtColour = IM_COL32(14, 14, 15, 255);
constexpr ImU32 kCapColour = IM_COL32(205, 200, 190, 255);
constexpr ImU32 kCapHighlight = IM_COL32(240, 238, 232, 255);
constexpr ImU32 kNotch = IM_COL32(40, 40, 42, 255);
constexpr ImU32 kHover = IM_COL32(255, 255, 255, 30);
constexpr ImU32 kLampOn = IM_COL32(235, 45, 35, 255);
constexpr ImU32 kLampHalo = IM_COL32(235, 45, 35, 70);
constexpr ImU32 kLampOff = IM_COL32(85, 22, 20, 255);
constexpr ImU32 kSlot = IM_COL32(10, 10, 11, 255);
constexpr ImU32 kSlider = IM_COL32(60, 60, 64, 255);
constexpr ImU32 kSliderTop = IM_COL32(110, 110, 116, 255);
constexpr ImU32 kTrimmerColour = IM_COL32(40, 41, 44, 255);
constexpr ImU32 kTrimmerSlot = IM_COL32(20, 20, 22, 255);

ImVec2 AtAngle(ImVec2 c, float r, float angle) {
    return {c.x + r * std::cos(angle), c.y + r * std::sin(angle)};
}

// Text centred on `centre`, `size` px high.
void Label(ImDrawList& list, ImVec2 centre, float size, const char* text) {
    const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text);
    list.AddText(ImGui::GetFont(), size, {centre.x - extent.x / 2, centre.y - extent.y / 2}, kLabel,
                 text);
}

}  // namespace

void DrawMonitor(ImDrawList& list, ImVec2 at, float width, const TvControls& controls,
                 const TvView* view, std::optional<TvKnob> hoveredKnob,
                 std::optional<ImVec2> loupe) {
    const float w = width;
    const float h = width * kTvAspect;
    const auto P = [&](float x, float y) { return ImVec2{at.x + x * w, at.y + y * w}; };

    // The shell and the front.
    list.AddRectFilled(at, {at.x + w, at.y + h}, kShell, 6.0f);
    list.AddRect(at, {at.x + w, at.y + h}, kShellEdge, 6.0f, 0, 1.0f);
    const ImVec2 frontMin = P(kShellInset, kShellInset);
    const ImVec2 frontMax{at.x + w - kShellInset * w, at.y + h};
    list.AddRectFilled(frontMin, frontMax, kFront, 0.03f * w, ImDrawFlags_RoundCornersTop);
    list.AddLine({frontMin.x + 0.03f * w, frontMin.y + 0.5f},
                 {frontMax.x - 0.03f * w, frontMin.y + 0.5f}, kFrontEdge, 1.0f);

    // The bezel, the screen and the picture.
    const ImVec2 screenMin = P(kScreenLeft, kScreenTop);
    const ImVec2 size{kScreenWidth * w, kScreenWidth * w * 0.75f};
    const ImVec2 screenMax{screenMin.x + size.x, screenMin.y + size.y};
    const float grow = kBezelGrow * w;
    list.AddRectFilled({screenMin.x - grow, screenMin.y - grow},
                       {screenMax.x + grow, screenMax.y + grow}, kBezel, 0.06f * w);
    struct Magnified {
        ImVec2 mouse, imageMin, imageMax;
    };
    std::optional<Magnified> magnified;  // drawn last, over the case
    list.PushClipRect(screenMin, screenMax, true);
    if (controls.on() && view != nullptr) {
        list.AddRectFilled(screenMin, screenMax, kBlack);
        const PictureWindow window = controls.window(view->lines());
        const PictureShift shift = controls.shift();
        const ImVec2 scale{size.x / (window.x1 - window.x0), size.y / (window.y1 - window.y0)};
        const ImVec2 imageMin{screenMin.x + shift.x * size.x - window.x0 * scale.x,
                              screenMin.y + shift.y * size.y - window.y0 * scale.y};
        const ImVec2 imageMax{imageMin.x + static_cast<float>(kTvColumns) * scale.x,
                              imageMin.y + static_cast<float>(kTvRows) * scale.y};
        list.AddImage(view->texture(), imageMin, imageMax);
        if (loupe && loupe->x >= screenMin.x && loupe->x < screenMax.x && loupe->y >= screenMin.y &&
            loupe->y < screenMax.y) {
            magnified = Magnified{*loupe, imageMin, imageMax};
        }
    } else {
        list.AddRectFilled(screenMin, screenMax, kGlass);
    }
    list.PopClipRect();
    // The screen's rounded corners: each square corner filled in the bezel's colour out to an arc
    // of radius r.
    const float r = kScreenRounding * w;
    const float pi = std::numbers::pi_v<float>;
    const struct {
        ImVec2 corner, centre;
        float from;
    } corners[] = {
        {screenMin, {screenMin.x + r, screenMin.y + r}, pi},
        {{screenMax.x, screenMin.y}, {screenMax.x - r, screenMin.y + r}, 1.5f * pi},
        {screenMax, {screenMax.x - r, screenMax.y - r}, 0},
        {{screenMin.x, screenMax.y}, {screenMin.x + r, screenMax.y - r}, 0.5f * pi},
    };
    const ImDrawListFlags flags = list.Flags;
    list.Flags &= ~ImDrawListFlags_AntiAliasedFill;  // no fringe along the screen's edges
    for (const auto& c : corners) {
        // A fan of triangles from the corner (the region is not convex).
        constexpr int kSteps = 12;
        ImVec2 last = AtAngle(c.centre, r, c.from);
        for (int k = 1; k <= kSteps; ++k) {
            const ImVec2 next =
                AtAngle(c.centre, r, c.from + 0.5f * pi * static_cast<float>(k) / kSteps);
            list.AddTriangleFilled(c.corner, last, next, kBezel);
            last = next;
        }
    }
    list.Flags = flags;
    list.AddLine({screenMin.x + r, screenMin.y + 1}, {screenMax.x - r, screenMin.y + 1}, kSheen,
                 1.0f);

    // The strip.
    const float font = kFont * w;
    list.AddRectFilled(P(kStripLeft, kStripTop), P(kStripRight, kStripBottom), kStrip, 0.01f * w);
    list.AddRect(P(kStripLeft, kStripTop), P(kStripRight, kStripBottom), kStripFrame, 0.01f * w, 0,
                 1.5f);

    // The knobs.
    for (const TvKnob knob : kTvKnobs) {
        const auto i = static_cast<std::size_t>(knob);
        const ImVec2 c = P(kKnobX[i], kKnobY);
        Label(list, P(kKnobX[i], kLabelY), font, ::bw::Label(knob));
        const bool hovered = hoveredKnob == knob;
        const float skirt = kSkirt * w, cap = kCap * w;
        list.AddCircleFilled(c, skirt, kSkirtColour, 32);
        list.AddCircleFilled(c, cap, kCapColour, 32);
        list.AddCircleFilled({c.x - cap * 0.3f, c.y - cap * 0.3f}, cap * 0.45f, kCapHighlight, 24);
        const float angle = kStart + static_cast<float>(controls.turn(knob)) * kSweep;
        list.AddLine(AtAngle(c, cap * 0.6f, angle), AtAngle(c, cap, angle), kNotch,
                     std::max(1.0f, 0.004f * w));
        if (hovered) {
            list.AddCircleFilled(c, skirt, kHover, 32);
        }
    }

    // The pilot lamp.
    const ImVec2 lamp = P(kLampX, kKnobY);
    if (controls.on()) {
        list.AddCircleFilled(lamp, 2 * kLamp * w, kLampHalo, 24);
        list.AddCircleFilled(lamp, kLamp * w, kLampOn, 16);
    } else {
        list.AddCircleFilled(lamp, kLamp * w, kLampOff, 16);
    }

    // The power switch: ON at the left, OFF at the right.
    const ImVec2 slotMin = P(kSlotLeft, kSlotTop), slotMax = P(kSlotRight, kSlotBottom);
    const float slotMidY = (slotMin.y + slotMax.y) / 2;
    Label(list, P((kSlotLeft + kSlotRight) / 2, kPowerY), font, "POWER");
    const ImVec2 on = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0, "ON");
    list.AddText(ImGui::GetFont(), font, {slotMin.x - 0.008f * w - on.x, slotMidY - on.y / 2},
                 kLabel, "ON");
    list.AddText(ImGui::GetFont(), font, {slotMax.x + 0.008f * w, slotMidY - on.y / 2}, kLabel,
                 "OFF");
    list.AddRectFilled(slotMin, slotMax, kSlot, 0.004f * w);
    const float half = (slotMax.x - slotMin.x) / 2;
    const float sliderX = controls.on() ? slotMin.x : slotMin.x + half;
    const float inset = 0.003f * w;
    list.AddRectFilled({sliderX + inset, slotMin.y + inset},
                       {sliderX + half - inset, slotMax.y - inset}, kSlider, 0.003f * w);
    list.AddLine({sliderX + inset, slotMin.y + inset + 0.5f},
                 {sliderX + half - inset, slotMin.y + inset + 0.5f}, kSliderTop, 1.0f);

    // The underscan trimmer: unlabelled, below the strip's right end.
    const ImVec2 trimmer = P(kTrimmerX, kTrimmerY);
    const float tr = kTrimmer * w;
    list.AddCircleFilled(trimmer, tr, kTrimmerColour, 16);
    const float slant = std::numbers::pi_v<float> / 6;
    list.AddLine(AtAngle(trimmer, tr, slant),
                 AtAngle(trimmer, tr, slant + std::numbers::pi_v<float>), kTrimmerSlot, 1.0f);
    // The loupe: the pixels under the mouse magnified, nearest-neighbour (each one a sharp square),
    // in a dark rim, the cursor hidden (the user, 2026-11-22).
    if (magnified) {
        const Magnified& m = *magnified;
        const ImVec2 span{m.imageMax.x - m.imageMin.x, m.imageMax.y - m.imageMin.y};
        const ImVec2 uv{(m.mouse.x - m.imageMin.x) / span.x, (m.mouse.y - m.imageMin.y) / span.y};
        const ImVec2 halfUv{kLoupeSource / 2 / span.x, kLoupeSource / 2 / span.y};
        const float radius = kLoupeSource * kLoupeZoom / 2;
        list.PushClipRectFullScreen();
        list.AddCircleFilled(m.mouse, radius, kBlack);
        const ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
        if (platform.DrawCallback_SetSamplerNearest != nullptr) {
            list.AddCallback(platform.DrawCallback_SetSamplerNearest, nullptr);
        }
        list.AddImageRounded(view->texture(), {m.mouse.x - radius, m.mouse.y - radius},
                             {m.mouse.x + radius, m.mouse.y + radius},
                             {uv.x - halfUv.x, uv.y - halfUv.y}, {uv.x + halfUv.x, uv.y + halfUv.y},
                             IM_COL32_WHITE, radius);
        if (platform.DrawCallback_SetSamplerLinear != nullptr) {
            list.AddCallback(platform.DrawCallback_SetSamplerLinear, nullptr);
        }
        list.AddCircle(m.mouse, radius, kLoupeRim, 0, 4.0f);
        list.AddCircle(m.mouse, radius - 3, kLoupeGlint, 0, 1.0f);
        list.PopClipRect();
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
}

void TvWindow::draw(bool& open, TvView& view) {
    view.setTone(controls_.tone());
    if (!open) {
        return;
    }
    PlaceNext(BenchObject::Tv, {kTvFirstWidth, kTvFirstWidth * kTvAspect});
    KeepAspect(kTvAspect, kMinWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kShell);
    const bool visible =
        ImGui::Begin(kTvWindowName, &open,
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
    const float w = ImGui::GetWindowWidth();
    const auto P = [&](float x, float y) { return ImVec2{at.x + x * w, at.y + y * w}; };

    // The controls first (their items), then the drawing (P7b: DrawMonitor draws only).
    std::optional<TvKnob> hoveredKnob;
    for (const TvKnob knob : kTvKnobs) {
        const auto i = static_cast<std::size_t>(knob);
        const ImVec2 c = P(kKnobX[i], kKnobY);
        const float hit = std::max(kKnobHit * w, kMinHit / 2);  // 44 px across at least (P8f)
        ImGui::SetCursorScreenPos({c.x - hit, c.y - hit});
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("##knob", {2 * hit, 2 * hit});
        ImGui::PopID();
        const KnobInput input = ReadKnob();
        if (input.turnBy != 0) {
            controls_.set(knob, controls_.turn(knob) + static_cast<float>(input.turnBy));
        }
        if (input.hovered) {
            hoveredKnob = knob;
            if (input.reset) {
                controls_.reset(knob);
            }
            ImGui::SetTooltip("%s %.0f %%\ndrag up or down, or the mouse wheel; double-click: back "
                              "to its mark",
                              ::bw::Label(knob), controls_.turn(knob) * 100.0);
        }
    }
    const ImVec2 slotMin = P(kSlotLeft, kSlotTop), slotMax = P(kSlotRight, kSlotBottom);
    const float slotGrow = std::max(0.0f, kMinHit - (slotMax.y - slotMin.y)) / 2;
    ImGui::SetCursorScreenPos({slotMin.x, slotMin.y - slotGrow});
    ImGui::InvisibleButton("##power",
                           {slotMax.x - slotMin.x, slotMax.y - slotMin.y + 2 * slotGrow});
    if (ImGui::IsItemClicked()) {
        controls_.setOn(!controls_.on());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("POWER: the monitor's own switch (the Machine's is on the shelf)");
    }
    const ImVec2 trimmer = P(kTrimmerX, kTrimmerY);
    const float trimmerHit = std::max(kTrimmerHit * w, kMinHit / 2);
    ImGui::SetCursorScreenPos({trimmer.x - trimmerHit, trimmer.y - trimmerHit});
    ImGui::InvisibleButton("##trimmer", {2 * trimmerHit, 2 * trimmerHit});
    if (ImGui::IsItemClicked()) {
        controls_.setUnderscan(!controls_.underscan());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(controls_.underscan()
                              ? "UNDERSCAN trimmer: the picture as a real monitor shows it"
                              : "UNDERSCAN trimmer: the whole frame");
    }
    // The loupe while the mouse is free over the window: no control under it, no button held
    // (a drag moves the case).
    const bool free = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
                      !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left);
    std::optional<ImVec2> loupe = free ? std::optional(io.MousePos) : std::nullopt;
    if (loupeAt_) {
        loupe = P(kScreenLeft + static_cast<float>(loupeAt_->x) * kScreenWidth,
                  kScreenTop + static_cast<float>(loupeAt_->y) * kScreenWidth * 0.75f);
    }
    DrawMonitor(list, at, w, controls_, &view, hoveredKnob, loupe);

    if (CaseClose()) {
        open = false;
    }
    PopOnDoubleClick(BenchObject::Tv);
    ImGui::End();
}

}  // namespace bw
