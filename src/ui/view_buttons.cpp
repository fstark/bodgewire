#include "ui/view_buttons.hpp"

#include <array>

#include "ui/bench.hpp"

namespace bw {

namespace {

constexpr ImU32 kPlate = IM_COL32(34, 36, 42, 235);
constexpr ImU32 kPlateHovered = IM_COL32(52, 55, 64, 245);
constexpr ImU32 kPlateEdge = IM_COL32(84, 88, 98, 255);
constexpr ImU32 kInk = IM_COL32(215, 216, 220, 255);
constexpr ImU32 kInkDim = IM_COL32(130, 132, 140, 255);
constexpr ImU32 kCopper = IM_COL32(110, 200, 90, 255);  // a Trace, as the Board's front copper
constexpr ImU32 kFold = IM_COL32(40, 160, 190, 255);    // the Flip Corner's turned side

struct Entry {
    ViewButton button;
    const char* tip;
};

// Bottom to top.
constexpr std::array<Entry, 5> kButtons{{
    {ViewButton::Flip, "Flip the Board (`)"},
    {ViewButton::Hold, "Hold: a click latches a button or a key down"},
    {ViewButton::Copper, "Copper: pick under the chips (Option)"},
    {ViewButton::Look, "Real or plain look (Tab)"},
    {ViewButton::Fit, "Fit the whole Board"},
}};

// Each drawing inside a 44 px cell centred on `c`; `s` scales it.
void DrawFit(ImDrawList& list, ImVec2 c, float s, ImU32 ink) {
    const float r = 13 * s, k = 6 * s;  // the frame's half side, a bracket's arm
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sy : {-1.0f, 1.0f}) {
            const ImVec2 corner{c.x + sx * r, c.y + sy * r};
            list.AddLine(corner, {corner.x - sx * k, corner.y}, ink, 2.0f);
            list.AddLine(corner, {corner.x, corner.y - sy * k}, ink, 2.0f);
        }
    }
    list.AddRectFilled({c.x - 7 * s, c.y - 5 * s}, {c.x + 7 * s, c.y + 5 * s}, kCopper, 1.0f);
}

// A chip: its body filled (the real look) or only outlined (the plain one).
void DrawChip(ImDrawList& list, ImVec2 c, float s, ImU32 ink, bool filled) {
    const ImVec2 a{c.x - 7 * s, c.y - 12 * s}, b{c.x + 7 * s, c.y + 12 * s};
    for (int i = 0; i < 4; ++i) {
        const float y = a.y + (3.0f + 6.0f * static_cast<float>(i)) * s;
        list.AddLine({a.x - 4 * s, y}, {a.x, y}, ink, 2.0f);
        list.AddLine({b.x, y}, {b.x + 4 * s, y}, ink, 2.0f);
    }
    if (filled) {
        list.AddRectFilled(a, b, ink, 2.0f);
    } else {
        list.AddRect(a, b, ink, 2.0f, 0, 1.5f);
    }
}

void DrawCopper(ImDrawList& list, ImVec2 c, float s, ImU32 ink) {
    // A Trace passing under a chip that is only outlined.
    list.AddLine({c.x - 15 * s, c.y + 8 * s}, {c.x - 3 * s, c.y + 8 * s}, kCopper, 3.0f);
    list.AddLine({c.x - 3 * s, c.y + 8 * s}, {c.x + 5 * s, c.y - 6 * s}, kCopper, 3.0f);
    list.AddLine({c.x + 5 * s, c.y - 6 * s}, {c.x + 15 * s, c.y - 6 * s}, kCopper, 3.0f);
    DrawChip(list, c, s, ink, false);
}

void DrawHold(ImDrawList& list, ImVec2 c, float s, ImU32 ink) {
    // A push button held down by a bar across it.
    list.AddRect({c.x - 12 * s, c.y - 2 * s}, {c.x + 12 * s, c.y + 12 * s}, ink, 2.0f, 0, 1.5f);
    list.AddCircleFilled({c.x, c.y + 5 * s}, 5 * s, ink, 24);
    list.AddLine({c.x - 10 * s, c.y - 8 * s}, {c.x + 10 * s, c.y - 8 * s}, ink, 2.5f);
    list.AddLine({c.x, c.y - 8 * s}, {c.x, c.y - 1 * s}, ink, 2.5f);
}

void DrawFlip(ImDrawList& list, ImVec2 c, float s, ImU32 ink) {
    // A Board with its bottom right corner turned over, as the Flip Corner.
    const ImVec2 a{c.x - 12 * s, c.y - 10 * s}, b{c.x + 12 * s, c.y + 10 * s};
    const float fold = 9 * s;
    const std::array<ImVec2, 5> outline{
        {a, {b.x, a.y}, {b.x, b.y - fold}, {b.x - fold, b.y}, {a.x, b.y}}};
    list.AddPolyline(outline.data(), static_cast<int>(outline.size()), ink, ImDrawFlags_Closed,
                     1.5f);
    list.AddTriangleFilled({b.x, b.y - fold}, {b.x - fold, b.y}, {b.x - fold, b.y - fold}, kFold);
}

}  // namespace

std::optional<ViewButton> ViewButtons::items(ImVec2 panelMin, ImVec2 panelMax, bool hold) {
    placed_.clear();
    hovered_ = false;
    std::optional<ViewButton> clicked;
    const float x = panelMax.x - kViewButtonMargin - kViewButton;
    float y = panelMax.y - kViewButtonMargin - kViewButton;
    ImGui::PushID("view buttons");
    for (const Entry& entry : kButtons) {
        if (entry.button == ViewButton::Hold && !hold) {
            continue;
        }
        if (y < panelMin.y) {
            break;  // a panel too short for them all
        }
        const ImVec2 min{x, y};
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(static_cast<int>(entry.button));
        if (ImGui::InvisibleButton("##button", {kViewButton, kViewButton})) {
            clicked = entry.button;
        }
        ImGui::PopID();
        const bool over = ImGui::IsItemHovered() || ImGui::IsItemActive();
        ImGui::SetItemTooltip("%s", entry.tip);
        hovered_ = hovered_ || over;
        placed_.push_back({entry.button, min, over});
        y -= kViewButton + kViewButtonGap;
    }
    ImGui::PopID();
    return clicked;
}

void ViewButtons::draw(ImDrawList& list, const ViewButtonsLook& look) const {
    for (const Placed& placed : placed_) {
        const ImVec2 min = placed.min, max{min.x + kViewButton, min.y + kViewButton};
        const ImVec2 c{(min.x + max.x) / 2, (min.y + max.y) / 2};
        const bool on = (placed.button == ViewButton::Look && look.plain) ||
                        (placed.button == ViewButton::Copper && look.copper) ||
                        (placed.button == ViewButton::Hold && look.hold) ||
                        (placed.button == ViewButton::Flip && look.back);
        list.AddRectFilled(min, max, placed.hovered ? kPlateHovered : kPlate, 6.0f);
        list.AddRect(min, max, on ? kFocusGold : kPlateEdge, 6.0f, 0, on ? 2.0f : 1.0f);
        const ImU32 ink = on ? kFocusGold : kInk;
        switch (placed.button) {
        case ViewButton::Fit:
            DrawFit(list, c, 1, ink);
            break;
        case ViewButton::Look:
            DrawChip(list, c, 1, look.plain ? ink : kInkDim, !look.plain);
            break;
        case ViewButton::Copper:
            DrawCopper(list, c, 1, ink);
            break;
        case ViewButton::Hold:
            DrawHold(list, c, 1, ink);
            break;
        case ViewButton::Flip:
            DrawFlip(list, c, 1, ink);
            break;
        }
    }
}

}  // namespace bw
