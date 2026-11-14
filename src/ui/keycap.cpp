#include "ui/keycap.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

namespace bw {

namespace {

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
        // A Board key's legend may be longer than one character (GO, DATA).
        const float share = cap.label.size() <= 1 ? 0.38f : cap.label.size() == 2 ? 0.30f : 0.22f;
        Centred(list, font, points(share), centre, a.y + height * 0.5f, cap.label);
    } else if (IsLetter(cap.label)) {
        Centred(list, font, points(0.24f), centre, a.y + height * 0.20f, cap.shifted);
        Centred(list, font, points(0.38f), centre, a.y + height * 0.66f, cap.label);
    } else {
        Centred(list, font, points(0.30f), centre, a.y + height * 0.29f, cap.shifted);
        Centred(list, font, points(0.30f), centre, a.y + height * 0.71f, cap.label);
    }
}

}  // namespace

void DrawKeyCap(ImDrawList& list, ImFont* font, const KeyCap& cap, ImVec2 a, ImVec2 b, float u,
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

}  // namespace bw
