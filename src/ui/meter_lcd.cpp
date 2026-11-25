#include "ui/meter_lcd.hpp"

#include <algorithm>
#include <array>
#include <numbers>
#include <vector>

namespace bw {

namespace {

constexpr ImU32 kCase = IM_COL32(230, 180, 30, 255);  // the Shelf drawing's yellow
constexpr ImU32 kCaseEdge = IM_COL32(150, 110, 15, 255);
constexpr ImU32 kBezel = IM_COL32(40, 40, 42, 255);
constexpr ImU32 kLcd = IM_COL32(168, 184, 158, 255);
constexpr ImU32 kLcdBeep = IM_COL32(150, 232, 130, 255);  // the backlight on a beep
constexpr ImU32 kInk = IM_COL32(28, 34, 28, 255);
constexpr ImU32 kGhost = IM_COL32(0, 0, 0, 20);  // an unlit segment
constexpr float kCellW = 22, kCellH = 34, kCellGap = 9, kThick = 4.5f;
constexpr float kSlant = 0.1f;  // the digits lean right, as an LCD's

// One segment between two points of the cell, a hexagon `kThick` wide, slanted about `baseY`.
void Segment(ImDrawList& list, ImVec2 a, ImVec2 b, float baseY, ImU32 colour) {
    const float h = kThick / 2;
    const bool across = a.y == b.y;
    std::array<ImVec2, 6> p;
    if (across) {
        p = {{{a.x, a.y}, {a.x + h, a.y - h}, {b.x - h, b.y - h}, {b.x, b.y}, {b.x - h, b.y + h},
              {a.x + h, a.y + h}}};
    } else {
        p = {{{a.x, a.y}, {a.x + h, a.y + h}, {b.x + h, b.y - h}, {b.x, b.y}, {b.x - h, b.y - h},
              {a.x - h, a.y + h}}};
    }
    for (ImVec2& q : p) {
        q.x += (baseY - q.y) * kSlant;
    }
    list.AddConvexPolyFilled(p.data(), static_cast<int>(p.size()), colour);
}

// A cell's seven segments at its top-left `at`, lit by `bits` (Segments), and its decimal point.
void Cell(ImDrawList& list, ImVec2 at, std::uint8_t bits, bool point) {
    const float l = at.x, r = at.x + kCellW, t = at.y, m = at.y + kCellH / 2, b = at.y + kCellH;
    const float g = 1.2f;  // the gap at each joint
    const std::array<std::pair<ImVec2, ImVec2>, 7> ends{{
        {{l + g, t}, {r - g, t}},  // a
        {{r, t + g}, {r, m - g}},  // b
        {{r, m + g}, {r, b - g}},  // c
        {{l + g, b}, {r - g, b}},  // d
        {{l, m + g}, {l, b - g}},  // e
        {{l, t + g}, {l, m - g}},  // f
        {{l + g, m}, {r - g, m}},  // g
    }};
    for (std::size_t s = 0; s < ends.size(); ++s) {
        const bool lit = (bits >> s) & 1;
        Segment(list, ends[s].first, ends[s].second, b, lit ? kInk : kGhost);
    }
    const ImVec2 dot{r + kCellGap / 2 - 1.5f, b - 1.5f};
    list.AddRectFilled({dot.x - 1.8f, dot.y - 1.8f}, {dot.x + 1.8f, dot.y + 1.8f},
                       point ? kInk : kGhost);
}

// The continuity sign: a dot and three sound waves, `at` the dot's centre.
void Beeper(ImDrawList& list, ImVec2 at) {
    constexpr float kPi = std::numbers::pi_v<float>;
    list.AddCircleFilled(at, 2.6f, kInk);
    for (int i = 1; i <= 3; ++i) {
        list.PathArcTo(at, 3.0f + 4.0f * static_cast<float>(i), -kPi / 4, kPi / 4, 8);
        list.PathStroke(kInk, ImDrawFlags_None, 1.8f);
    }
}

}  // namespace

void DrawMeterLcd(ImDrawList& list, ImVec2 base, const MeterReading& reading) {
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    const float half = kMeterLcdSize.x / 2;
    const float x = std::clamp(base.x, viewport.Pos.x + half + 4,
                               viewport.Pos.x + viewport.Size.x - half - 4);
    const ImVec2 min{x - half, base.y - kMeterLcdSize.y};
    const ImVec2 max{x + half, base.y};
    // The case's top, rising from the shelf: its upper corners round, a shadow at its right.
    list.AddRectFilled({min.x + 3, min.y + 3}, {max.x + 3, max.y}, IM_COL32(0, 0, 0, 90), 10.0f,
                       ImDrawFlags_RoundCornersTop);
    list.AddRectFilled(min, max, kCase, 10.0f, ImDrawFlags_RoundCornersTop);
    list.AddRect(min, max, kCaseEdge, 10.0f, ImDrawFlags_RoundCornersTop, 1.5f);
    const ImVec2 bezelMin{min.x + 8, min.y + 7}, bezelMax{max.x - 8, max.y - 5};
    list.AddRectFilled(bezelMin, bezelMax, kBezel, 4.0f);
    const ImVec2 lcdMin{bezelMin.x + 4, bezelMin.y + 4}, lcdMax{bezelMax.x - 4, bezelMax.y - 4};
    list.AddRectFilled(lcdMin, lcdMax, reading.beep ? kLcdBeep : kLcd, 2.0f);

    // The cells, right-aligned: the characters with the decimal point each one lights.
    struct Shown {
        char c = ' ';
        bool point = false;
    };
    std::vector<Shown> cells;
    for (const char c : reading.digits) {
        if (c == '.' && !cells.empty()) {
            cells.back().point = true;
        } else {
            cells.push_back({c});
        }
    }
    while (cells.size() < kMeterCells) {
        cells.insert(cells.begin(), Shown{});
    }
    const float top = (lcdMin.y + lcdMax.y - kCellH) / 2;
    float left = lcdMin.x + 10;
    for (const Shown& cell : cells) {
        Cell(list, {left, top}, Segments(cell.c), cell.point);
        left += kCellW + kCellGap;
    }
    // The mode's sign at the right.
    const ImVec2 sign{lcdMax.x - 16, (lcdMin.y + lcdMax.y) / 2};
    if (reading.continuity) {
        Beeper(list, {sign.x - 6, sign.y});
    } else {
        ImFont* font = ImGui::GetFont();
        const float size = 22.0f;
        const ImVec2 v = ImGui::CalcTextSize("V");
        const float scale = size / ImGui::GetFontSize();
        list.AddText(font, size, {sign.x - v.x * scale / 2, sign.y - v.y * scale / 2}, kInk, "V");
    }
}

}  // namespace bw
