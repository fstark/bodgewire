#include "ui/case_keys.hpp"

#include <string>

#include "ui/bw_logo.hpp"
#include "ui/controls.hpp"
#include "ui/instrument_marks.hpp"
#include "view/time_view.hpp"

namespace bw {

namespace {

constexpr ImU32 kFrameEdge = IM_COL32(29, 30, 32, 255);
constexpr ImU32 kGroupPanel = IM_COL32(207, 213, 220, 255);
constexpr ImU32 kInk = IM_COL32(43, 47, 54, 255);
constexpr ImU32 kInkFaint = IM_COL32(90, 95, 104, 255);
constexpr ImU32 kCap = IM_COL32(239, 238, 233, 255);
constexpr ImU32 kCapPressed = IM_COL32(220, 219, 213, 255);
constexpr ImU32 kCapEdge = IM_COL32(169, 166, 158, 255);
constexpr ImU32 kCapShadow = IM_COL32(156, 154, 147, 255);
constexpr ImU32 kBezelFill = IM_COL32(38, 40, 43, 255);
constexpr ImU32 kBezelEdge = IM_COL32(17, 17, 17, 255);
constexpr ImU32 kGridCurrent = IM_COL32(61, 122, 74, 255);
constexpr float kFrameWidth = 6;

ImVec2 Min(ImVec2 at, const Box& b) { return {at.x + b.x0, at.y + b.y0}; }
ImVec2 Max(ImVec2 at, const Box& b) { return {at.x + b.x1, at.y + b.y1}; }

void Centred(ImDrawList& list, ImVec2 at, const Box& b, const char* text, ImU32 colour) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    list.AddText({at.x + (b.x0 + b.x1 - size.x) / 2, at.y + (b.y0 + b.y1 - size.y) / 2}, colour,
                 text);
}

// A key cap's body: its face 2 px above its shadow, or, pressed, down on it. Returns the face.
Box Cap(ImDrawList& list, ImVec2 at, const Box& key, bool pressed) {
    const Box face =
        pressed ? Box{key.x0, key.y0 + 2, key.x1, key.y1} : Box{key.x0, key.y0, key.x1, key.y1 - 2};
    if (!pressed) {
        list.AddRectFilled(Min(at, {key.x0, key.y0 + 2, key.x1, key.y1}), Max(at, key), kCapShadow,
                           4);
    }
    list.AddRectFilled(Min(at, face), Max(at, face), pressed ? kCapPressed : kCap, 4);
    list.AddRect(Min(at, face), Max(at, face), kCapEdge, 4);
    return face;
}

// A value of `values` in its row's unit: 500 for 500 ns, 2 for 2 µs.
Time InUnit(Time t) {
    return t / (t < 1'000'000           ? 1'000
                : t < 1'000'000'000     ? 1'000'000
                : t < 1'000'000'000'000 ? 1'000'000'000
                                        : 1'000'000'000'000);
}

}  // namespace

void DrawCaseFrame(ImDrawList& list, ImVec2 at, float width, float height) {
    const ImVec2 max{at.x + width, at.y + height};
    list.AddRectFilled(at, max, kCaseFrame, 10);
    list.AddRect(at, max, kFrameEdge, 10, 0, 1.5f);
    list.AddRectFilled({at.x + kFrameWidth, at.y + kFrameWidth},
                       {max.x - kFrameWidth, max.y - kFrameWidth}, kCasePanel, 6);
}

void DrawGroup(ImDrawList& list, ImVec2 at, const Box& group) {
    list.AddRectFilled(Min(at, group), Max(at, group), kGroupPanel, 5);
}

void DrawGroupLabel(ImDrawList& list, ImVec2 at, float x0, float x1, float y, const char* text) {
    const float size = ImGui::GetFontSize() * 0.78f;
    const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text);
    list.AddText(nullptr, size, {at.x + (x0 + x1 - extent.x) / 2, at.y + y}, kInk, text);
}

void DrawBadge(ImDrawList& list, ImVec2 at, const Badge& badge) {
    DrawBwLogo(list, Min(at, badge.logo), badge.logo.height());
    if (badge.hasWords) {
        const float size = ImGui::GetFontSize() * 0.85f;
        list.AddText(nullptr, size, {at.x + badge.words.x0, at.y + badge.words.y0}, kInk, "BODGE");
        list.AddText(nullptr, size, {at.x + badge.words.x0, at.y + badge.words.y1 - size}, kInk,
                     "WIRE");
    }
}

void DrawBezel(ImDrawList& list, ImVec2 at, const Box& bezel) {
    list.AddRectFilled(Min(at, bezel), Max(at, bezel), kBezelFill, 8);
    list.AddRect(Min(at, bezel), Max(at, bezel), kBezelEdge, 8, 0, 1.5f);
}

void DrawKey(ImDrawList& list, ImVec2 at, const Box& key, const char* label, bool pressed) {
    Centred(list, at, Cap(list, at, key, pressed), label, kInk);
}

void DrawArrowKey(ImDrawList& list, ImVec2 at, const Box& key, bool right, bool pressed) {
    const Box face = Cap(list, at, key, pressed);
    const float cx = at.x + (face.x0 + face.x1) / 2, cy = at.y + (face.y0 + face.y1) / 2;
    const float h = 6, w = 5;
    if (right) {
        list.AddTriangleFilled({cx - w, cy - h}, {cx + w, cy}, {cx - w, cy + h}, kInk);
    } else {
        list.AddTriangleFilled({cx + w, cy - h}, {cx - w, cy}, {cx + w, cy + h}, kInk);
    }
}

void DrawSizeKey(ImDrawList& list, ImVec2 at, const Box& key, bool pressed) {
    const Box face = Cap(list, at, key, pressed);
    const float side = 24;
    DrawSizeIcon(list,
                 {at.x + (face.x0 + face.x1 - side) / 2, at.y + (face.y0 + face.y1 - side) / 2},
                 side, kInk);
}

void DrawLed(ImDrawList& list, ImVec2 at, const Box& window, const char* text) {
    list.AddRectFilled(Min(at, window), Max(at, window), kLedFrame, 3);
    list.AddRect(Min(at, window), Max(at, window), IM_COL32(85, 85, 85, 255), 3);
    Centred(list, at, window, text, kLedText);
}

void DrawLamp(ImDrawList& list, ImVec2 at, const Box& lamp, bool lit, const char* name) {
    const ImVec2 c{at.x + (lamp.x0 + lamp.x1) / 2, at.y + (lamp.y0 + lamp.y1) / 2};
    const float r = lamp.width() / 2 - 2;
    if (lit) {
        list.AddCircleFilled(c, r + 5, IM_COL32(60, 224, 60, 50));
        list.AddCircleFilled(c, r, IM_COL32(60, 224, 60, 255));
        list.AddCircleFilled({c.x - r * 0.3f, c.y - r * 0.3f}, r * 0.35f,
                             IM_COL32(200, 255, 176, 255));
    } else {
        list.AddCircleFilled(c, r, IM_COL32(31, 58, 31, 255));
    }
    list.AddCircle(c, r, IM_COL32(34, 34, 34, 255), 0, 1.0f);
    DrawGroupLabel(list, at, lamp.x0 - 8, lamp.x1 + 8, lamp.y1 + 2, name);
}

void DrawRocker(ImDrawList& list, ImVec2 at, std::span<const Box> halves,
                std::span<const char* const> names, std::size_t lit) {
    const Box& first = halves.front();
    const Box whole{first.x0, first.y0 + 6, halves.back().x1, first.y1 - 6};
    list.AddRectFilled(Min(at, whole), Max(at, whole), kCapShadow, 5);
    const Box& raised = halves[lit];
    const Box face{raised.x0 + 2, whole.y0 + 2, raised.x1 - 2, whole.y1 - 2};
    list.AddRectFilled(Min(at, face), Max(at, face), kCap, 4);
    list.AddRect(Min(at, face), Max(at, face), kCapEdge, 4);
    for (std::size_t i = 0; i < halves.size(); ++i) {
        Centred(list, at, halves[i], names[i], i == lit ? kInk : kInkFaint);
    }
}

std::optional<Time> TimeGrid(const char* id, std::span<const Time> values, Time current,
                             ImVec2 below) {
    std::optional<Time> chosen;
    ImGui::SetNextWindowPos(below);
    if (!ImGui::BeginPopup(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
        return chosen;
    }
    for (const UnitRow& row : UnitRows(values)) {
        ImGui::PushID(row.unit);
        ImGui::AlignTextToFramePadding();
        BigText(row.unit);
        for (const std::size_t i : row.indices) {
            ImGui::SameLine(i == row.indices.front() ? 40.0f : 0.0f);
            const Time t = values[i];
            const bool lit = t == current;
            if (lit) {
                ImGui::PushStyleColor(ImGuiCol_Button, kGridCurrent);
            }
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button(std::to_string(InUnit(t)).c_str(), {kBig, kBig})) {
                chosen = t;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (lit) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopID();
    }
    ImGui::EndPopup();
    return chosen;
}

}  // namespace bw
