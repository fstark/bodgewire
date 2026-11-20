#include "ui/shelf_drawings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

namespace bw {

namespace {

// A cell's drawing coordinates: 0..56 on each side of a square cell (x to 112 for the probe).
class Pen {
public:
    Pen(ImDrawList& list, ImVec2 min, ImVec2 max, float alpha)
        : list_(list), min_(min), unit_((max.y - min.y) / 56.0f), alpha_(alpha) {}

    ImVec2 at(float x, float y) const { return {min_.x + x * unit_, min_.y + y * unit_}; }
    float size(float s) const { return s * unit_; }
    ImU32 colour(ImU32 c) const {
        const auto a = static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha_;
        return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
    }

    void box(float x0, float y0, float x1, float y1, ImU32 c, float rounding = 0) {
        list_.AddRectFilled(at(x0, y0), at(x1, y1), colour(c), size(rounding));
    }
    void frame(float x0, float y0, float x1, float y1, ImU32 c, float thick, float rounding = 0) {
        list_.AddRect(at(x0, y0), at(x1, y1), colour(c), size(rounding), 0, size(thick));
    }
    void disc(float x, float y, float r, ImU32 c) {
        list_.AddCircleFilled(at(x, y), size(r), colour(c));
    }
    void ring(float x, float y, float r, ImU32 c, float thick) {
        list_.AddCircle(at(x, y), size(r), colour(c), 0, size(thick));
    }
    void line(float x0, float y0, float x1, float y1, ImU32 c, float thick) {
        list_.AddLine(at(x0, y0), at(x1, y1), colour(c), size(thick));
    }
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, ImU32 c) {
        list_.AddTriangleFilled(at(x0, y0), at(x1, y1), at(x2, y2), colour(c));
    }
    void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
              ImU32 c) {
        list_.AddQuadFilled(at(x0, y0), at(x1, y1), at(x2, y2), at(x3, y3), colour(c));
    }
    void ellipse(float x, float y, float rx, float ry, ImU32 c, float thick) {
        list_.AddEllipse(at(x, y), {size(rx), size(ry)}, colour(c), 0, 0, size(thick));
    }
    // Text of `height` units, centred on (x, y).
    void text(float x, float y, float height, ImU32 c, const char* s) {
        ImFont* font = ImGui::GetFont();
        const float px = size(height);
        const ImVec2 extent = font->CalcTextSizeA(px, 1e9f, 0, s);
        const ImVec2 centre = at(x, y);
        list_.AddText(font, px, {centre.x - extent.x / 2, centre.y - extent.y / 2}, colour(c), s);
    }
    void polyline(std::span<const ImVec2> points, ImU32 c, float thick) {
        std::array<ImVec2, 32> screen{};
        const std::size_t n = std::min(points.size(), screen.size());
        for (std::size_t i = 0; i < n; ++i) {
            screen[i] = at(points[i].x, points[i].y);
        }
        list_.AddPolyline(screen.data(), static_cast<int>(n), colour(c), 0, size(thick));
    }

private:
    ImDrawList& list_;
    ImVec2 min_;
    float unit_;
    float alpha_;
};

constexpr ImU32 kMetal = IM_COL32(170, 172, 176, 255);
constexpr ImU32 kDarkMetal = IM_COL32(95, 98, 104, 255);
constexpr ImU32 kCase = IM_COL32(205, 200, 188, 255);  // beige instrument cases
constexpr ImU32 kCaseEdge = IM_COL32(120, 115, 105, 255);
constexpr ImU32 kBlack = IM_COL32(22, 22, 24, 255);
constexpr ImU32 kScreenOff = IM_COL32(30, 36, 34, 255);
constexpr ImU32 kScreenOn = IM_COL32(20, 70, 45, 255);
constexpr ImU32 kTrace = IM_COL32(120, 255, 160, 255);
constexpr ImU32 kTvOn = IM_COL32(150, 175, 205, 255);
constexpr ImU32 kLampDark = IM_COL32(60, 60, 60, 255);
constexpr ImU32 kGreen = IM_COL32(60, 210, 90, 255);
constexpr ImU32 kRed = IM_COL32(230, 45, 40, 255);
constexpr ImU32 kAmber = IM_COL32(255, 170, 0, 255);
constexpr ImU32 kCopper = IM_COL32(205, 120, 60, 255);
constexpr ImU32 kWhite = IM_COL32(235, 235, 235, 255);
constexpr ImU32 kGrey = IM_COL32(150, 150, 150, 255);

void Power(Pen& p, const ItemLook& look) {
    p.box(10, 6, 46, 50, IM_COL32(60, 60, 64, 255), 4);
    p.frame(10, 6, 46, 50, IM_COL32(110, 110, 115, 255), 1, 4);
    p.disc(28, 28, 7, kBlack);
    // The lever from its bush, up when on.
    const float tipY = look.lit ? 11 : 45;
    p.line(28, 28, 28, tipY, kMetal, 4);
    p.disc(28, tipY, 3.5f, kWhite);
    p.text(28, look.lit ? 45 : 11, 9, look.lit ? kGreen : kGrey, look.lit ? "ON" : "OFF");
    p.disc(41, 45, 3, look.limit ? kRed : kLampDark);  // LIMIT
}

// A three-position slide across two cells: a pause sign, 1×, Max printed over its slot, the slider
// at the chosen third; the pause sign grayed while the power is off (it cannot be chosen).
void RunSlide(Pen& p, const ItemLook& look) {
    constexpr std::array<float, 3> kAt{22, 58, 94};
    const auto chosen = static_cast<std::size_t>(look.run);
    const ImU32 pauseInk = !look.powered ? kLampDark : chosen == 0 ? kWhite : kGrey;
    p.box(kAt[0] - 5, 8, kAt[0] - 1.5f, 20, pauseInk, 0.5f);
    p.box(kAt[0] + 1.5f, 8, kAt[0] + 5, 20, pauseInk, 0.5f);
    p.text(kAt[1], 14, 11, chosen == 1 ? kWhite : kGrey, "1×");
    p.text(kAt[2], 14, 11, chosen == 2 ? kWhite : kGrey, "Max");
    p.box(10, 31, 106, 41, kBlack, 5);
    for (const float x : kAt) {
        p.line(x, 43.5f, x, 46, kDarkMetal, 1.2f);
    }
    const float x = kAt[chosen];
    p.box(x - 9, 28, x + 9, 44, kMetal, 3);
    p.line(x, 30, x, 42, kDarkMetal, 1.5f);
}

void Probe(Pen& p, const ItemLook& look) {
    // A pen lying with its tip to the left, three lamps on its body, the cable to the right.
    p.line(4, 28, 18, 28, kMetal, 1.5f);
    p.triangle(18, 23, 18, 33, 26, 28, kDarkMetal);
    p.box(24, 19, 98, 37, IM_COL32(40, 40, 46, 255), 9);
    p.frame(24, 19, 98, 37, IM_COL32(90, 90, 98, 255), 1, 9);
    p.line(98, 28, 108, 34, kBlack, 3);
    struct Lamp {
        float x;
        bool lit;
        ImU32 colour;
        const char* name;
    };
    const std::array<Lamp, 3> lamps{{{44, look.lamps.high, kRed, "H"},
                                     {61, look.lamps.low, kGreen, "L"},
                                     {78, look.lamps.pulse, kAmber, "P"}}};
    for (const Lamp& lamp : lamps) {
        if (lamp.lit) {
            p.disc(lamp.x, 28, 7.5f, (lamp.colour & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 80));
        }
        p.disc(lamp.x, 28, 5, lamp.lit ? lamp.colour : kLampDark);
        p.text(lamp.x, 45, 9, kGrey, lamp.name);
    }
}

void Puller(Pen& p, const ItemLook& look) {
    // A chip extractor: two prongs from a bow, hooked under a DIP's ends; held (lit), the chip
    // lifted off an empty socket.
    const float lift = look.lit ? 5 : 0;
    if (look.lit) {
        p.box(14, 47, 42, 51, IM_COL32(70, 70, 76, 255), 1);
    }
    p.line(14, 10, 42, 10, kMetal, 3);
    p.line(14, 10, 12, 36 - lift, kMetal, 3);
    p.line(42, 10, 44, 36 - lift, kMetal, 3);
    p.line(12, 36 - lift, 15, 39 - lift, kMetal, 3);
    p.line(44, 36 - lift, 41, 39 - lift, kMetal, 3);
    p.box(15, 34 - lift, 41, 44 - lift, kBlack, 1);
    for (float x = 18; x < 40; x += 5) {
        p.line(x, 44 - lift, x, 49 - lift, kMetal, 1.5f);
    }
}

void Iron(Pen& p, const ItemLook& look) {
    // Handle bottom right, the shaft and copper tip to the upper left; held (lit), the tip glowing
    // hot, a drop of solder beside it.
    constexpr ImU32 kHot = IM_COL32(255, 120, 30, 255);
    p.line(48, 48, 32, 32, IM_COL32(40, 70, 140, 255), 9);
    p.line(33, 33, 30, 30, kDarkMetal, 6);
    p.line(30, 30, 15, 15, kMetal, 3);
    if (look.lit) {
        p.disc(11, 11, 6, IM_COL32(255, 120, 30, 70));
    }
    p.line(15, 15, 9, 9, look.lit ? kHot : kCopper, 2.5f);
    if (look.lit) {
        p.disc(11, 18, 3, IM_COL32(200, 200, 210, 255));
    }
}

void Cutters(Pen& p, const ItemLook& look) {
    // Two jaws meeting at the top, a pivot, two red handles spreading down; held (lit), the jaws
    // open and the handles wider.
    const float open = look.lit ? 6 : 0;
    p.quad(28 - open, 6, 22 - open / 2, 14, 25, 26, 28, 24, kDarkMetal);
    p.quad(28 + open, 6, 34 + open / 2, 14, 31, 26, 28, 24, kMetal);
    p.disc(28, 26, 3.5f, kDarkMetal);
    p.line(27, 28, 15 - open / 2, 50, kRed, 6);
    p.line(29, 28, 41 + open / 2, 50, kRed, 6);
}

void Wire(Pen& p, const ItemLook& look) {
    // A coil of green wire, its end stripped to copper at the upper left (clear of the tooltip);
    // held (lit), the end tinned, a bead of solder on it.
    for (int i = 0; i < 3; ++i) {
        const float x = 26 + static_cast<float>(i) * 5;
        p.ellipse(x, 28, 10, 16, IM_COL32(40, 170, 70, 255), 2.5f);
    }
    p.line(16, 24, 8, 12, IM_COL32(40, 170, 70, 255), 2.5f);
    p.line(8, 12, 5, 7, look.lit ? kMetal : kCopper, 2);
    if (look.lit) {
        p.disc(4.5f, 6.5f, 2.5f, IM_COL32(200, 200, 210, 255));
    }
}

void Pencil(Pen& p, const ItemLook& look) {
    // A white china marker, paper-wrapped, diagonal as the iron: its wax tip to the upper left,
    // the pull string trailing; held (lit), a grey check scribbled below left.
    constexpr ImU32 kWax = IM_COL32(215, 215, 210, 255);
    p.line(46, 46, 22, 22, kWhite, 9);
    for (float t = 26; t < 44; t += 5) {  // the paper's spiral
        p.line(t - 3, t + 3, t + 3, t - 3, kGrey, 1.2f);
    }
    p.line(22, 22, 14, 14, kWax, 5);
    p.disc(13.5f, 13.5f, 2.5f, kWax);
    p.line(44, 40, 51, 33, kDarkMetal, 1);  // the string
    if (look.lit) {
        const std::array<ImVec2, 3> check{{{8, 41}, {13, 47}, {22, 36}}};
        p.polyline(check, kWax, 2.5f);
    }
}

void Tv(Pen& p, const ItemLook& look) {
    // A small 1970s monitor: a cream shell, a dark front, the screen high, a strip of knobs.
    p.box(7, 6, 49, 47, IM_COL32(220, 214, 194, 255), 4);
    p.box(9, 8, 47, 47, IM_COL32(50, 50, 54, 255), 3);
    p.box(12, 10, 44, 34, look.lit ? kTvOn : kScreenOff, 3);
    p.box(11, 37, 45, 44, IM_COL32(24, 24, 26, 255), 1);
    for (int i = 0; i < 4; ++i) {
        p.disc(15 + 4.5f * static_cast<float>(i), 40.5f, 1.3f, IM_COL32(200, 196, 186, 255));
    }
    p.disc(33, 40.5f, 1.2f, look.lit ? IM_COL32(235, 45, 35, 255) : IM_COL32(85, 22, 20, 255));
    p.line(14, 47, 14, 50, kDarkMetal, 2);
    p.line(42, 47, 42, 50, kDarkMetal, 2);
}

void Keyboard(Pen& p, const ItemLook& look) {
    p.quad(4, 22, 52, 22, 54, 46, 2, 46, IM_COL32(60, 60, 64, 255));
    for (int row = 0; row < 3; ++row) {
        const float y = 25 + static_cast<float>(row) * 6.5f;
        const float inset = static_cast<float>(row) * 2;
        for (float x = 8 + inset; x < 48; x += 5.5f) {
            p.box(x, y, x + 4, y + 4.5f, kCase, 0.8f);
        }
    }
    p.box(14, 41.5f, 42, 44.5f, kCase, 0.8f);  // space bar
    p.disc(48, 15, 3, look.lit ? kGreen : kLampDark);
}

// A beige bench scope: a square screen with a graticule and a sine, knobs down the right, two
// probe sockets under the screen.
void Scope(Pen& p, const ItemLook& look) {
    p.box(5, 6, 51, 48, kCase, 3);
    p.frame(5, 6, 51, 48, kCaseEdge, 1, 3);
    p.box(9, 10, 37, 36, look.lit ? kScreenOn : kScreenOff, 3);
    if (look.lit) {
        p.line(23, 11, 23, 35, IM_COL32(60, 120, 80, 255), 0.8f);
        p.line(10, 23, 36, 23, IM_COL32(60, 120, 80, 255), 0.8f);
        std::array<ImVec2, 25> sine{};
        for (std::size_t i = 0; i < sine.size(); ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sine.size() - 1);
            sine[i] = {11 + 24 * t, 23 - 8 * std::sin(t * 2 * std::numbers::pi_v<float> * 1.5f)};
        }
        p.polyline(sine, kTrace, 1.5f);
    }
    for (float y = 14; y < 36; y += 9) {
        p.disc(44, y, 3.5f, kDarkMetal);
        p.line(44, y, 44, y - 3, kWhite, 1);
    }
    p.ring(15, 42, 2.5f, kDarkMetal, 1.5f);
    p.ring(25, 42, 2.5f, kDarkMetal, 1.5f);
    p.disc(44, 43, 1.8f, look.lit ? kGreen : kLampDark);  // pilot lamp
}

// A dark, low logic analyzer: a wide screen of coloured lanes, its pod's coloured clip leads
// fanning out below.
void Analyzer(Pen& p, const ItemLook& look) {
    constexpr std::array<ImU32, 6> kLeads{
        IM_COL32(255, 230, 0, 255), IM_COL32(0, 230, 255, 255),   IM_COL32(255, 60, 220, 255),
        IM_COL32(255, 140, 0, 255), IM_COL32(140, 255, 100, 255), IM_COL32(110, 160, 255, 255)};
    for (std::size_t i = 0; i < kLeads.size(); ++i) {
        const float x0 = 20 + static_cast<float>(i) * 3;
        const float x1 = 8 + static_cast<float>(i) * 8;
        p.line(x0, 34, x1, 47, kLeads[i], 1.5f);
        p.box(x1 - 1.5f, 47, x1 + 1.5f, 52, kLeads[i], 0.5f);
    }
    p.box(3, 10, 53, 34, IM_COL32(45, 52, 66, 255), 3);
    p.frame(3, 10, 53, 34, IM_COL32(90, 100, 120, 255), 1, 3);
    p.box(6, 13, 44, 31, look.lit ? IM_COL32(12, 16, 24, 255) : kScreenOff, 1.5f);
    if (look.lit) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const float hi = 14.5f + static_cast<float>(lane) * 4, lo = hi + 2.5f;
            const float shift = static_cast<float>(lane) * 2;
            const std::array<ImVec2, 8> square{{{7, lo},
                                                {13 + shift, lo},
                                                {13 + shift, hi},
                                                {22 + shift, hi},
                                                {22 + shift, lo},
                                                {32 - shift, lo},
                                                {32 - shift, hi},
                                                {43, hi}}};
            p.polyline(square, kLeads[lane], 1.0f);
        }
    }
    p.box(47, 14, 50, 17, kDarkMetal, 0.5f);
    p.box(47, 20, 50, 23, kDarkMetal, 0.5f);
    p.disc(48.5f, 29, 1.6f, look.lit ? kGreen : kLampDark);  // pilot lamp
}

void Meter(Pen& p, const ItemLook& look) {
    p.box(14, 4, 42, 52, IM_COL32(230, 180, 30, 255), 5);
    p.box(17, 8, 39, 19, look.lit ? IM_COL32(170, 200, 160, 255) : kScreenOff, 1);
    if (look.lit) {
        p.text(28, 13.5f, 9, kBlack, "5.0");
    }
    p.disc(28, 31, 7, kBlack);
    p.line(28, 31, 28, 25, kWhite, 1.5f);
    p.disc(21, 46, 2.2f, kRed);
    p.disc(35, 46, 2.2f, kBlack);
}

// The manuals binder: closed, a blue ring binder with its dividers' tabs; out, open flat.
void Manual(Pen& p, const ItemLook& look) {
    constexpr ImU32 kCover = IM_COL32(40, 70, 130, 255);
    constexpr ImU32 kSpine = IM_COL32(25, 45, 90, 255);
    constexpr ImU32 kPaper = IM_COL32(246, 242, 228, 255);
    constexpr ImU32 kRing = IM_COL32(190, 190, 198, 255);
    constexpr ImU32 kYellow = IM_COL32(240, 225, 140, 255);
    constexpr ImU32 kGreenTab = IM_COL32(170, 215, 160, 255);
    constexpr ImU32 kPink = IM_COL32(235, 175, 190, 255);
    if (look.lit) {  // open flat, the rings across the middle
        p.box(5, 12, 51, 46, kCover, 2);
        p.box(7, 14, 27, 44, kPaper, 1);
        p.box(29, 14, 49, 44, kPaper, 1);
        p.box(49, 18, 52, 24, kYellow, 1);
        for (float y = 19; y < 42; y += 5) {
            p.line(10, y, 24, y, kGrey, 1);
            p.line(32, y, 46, y, kGrey, 1);
        }
        for (const float y : {18.0f, 29.0f, 40.0f}) {
            p.ring(28, y, 3, kRing, 1.5f);
        }
    } else {
        p.box(40, 11, 45, 17, kYellow, 1);
        p.box(40, 21, 45, 27, kGreenTab, 1);
        p.box(40, 31, 45, 37, kPink, 1);
        p.box(12, 8, 42, 48, kCover, 2);
        p.box(12, 8, 17, 48, kSpine, 1);
        for (const float y : {16.0f, 28.0f, 40.0f}) {
            p.line(13, y, 16, y, kRing, 2);
        }
        p.box(23, 16, 37, 22, kWhite, 1);
    }
}

void Knob(Pen& p, const ItemLook& look) {
    p.disc(28, 30, 17, IM_COL32(50, 50, 54, 255));
    p.ring(28, 30, 17, kDarkMetal, 1.5f);
    p.disc(28, 30, 12, IM_COL32(30, 30, 33, 255));
    p.line(28, 30, 37, 21, kWhite, 2.5f);
    p.disc(47, 9, 3, look.lit ? kGreen : kLampDark);
}

// A yellow note, slightly turned, a folded corner, three lines of writing.
void Brief(Pen& p, const ItemLook& look) {
    const ImU32 paper = look.lit ? IM_COL32(255, 232, 90, 255) : IM_COL32(200, 180, 80, 255);
    p.quad(11, 13, 44, 10, 46, 40, 13, 45, paper);
    p.triangle(37, 41, 46, 40, 45, 32, IM_COL32(170, 150, 60, 255));
    for (float y = 20; y < 36; y += 6) {
        p.line(16, y + 1, 39, y - 1, IM_COL32(90, 80, 50, 255), 1.2f);
    }
}

}  // namespace

void DrawItem(ImDrawList& list, Item item, ImVec2 min, ImVec2 max, const ItemLook& look) {
    Pen p(list, min, max, look.alpha);
    switch (item) {
    case Item::Power:
        Power(p, look);
        break;
    case Item::RunSwitch:
        RunSlide(p, look);
        break;
    case Item::Probe:
        Probe(p, look);
        break;
    case Item::Puller:
        Puller(p, look);
        break;
    case Item::Iron:
        Iron(p, look);
        break;
    case Item::Cutters:
        Cutters(p, look);
        break;
    case Item::Wire:
        Wire(p, look);
        break;
    case Item::Pencil:
        Pencil(p, look);
        break;
    case Item::Analyzer:
        Analyzer(p, look);
        break;
    case Item::Scope:
        Scope(p, look);
        break;
    case Item::Meter:
        Meter(p, look);
        break;
    case Item::Manual:
        Manual(p, look);
        break;
    case Item::Machine: {
        // One cell a device, TV, keyboard, knob box, each lit while out; none: the monitor alone.
        const SlotLook& slot = look.slot;
        const std::array<bool, 3> has{slot.tv || (!slot.keyboard && !slot.knob), slot.keyboard,
                                      slot.knob};
        const std::array<bool, 3> lit{slot.tvLit, slot.keyboardLit, slot.knobLit};
        const float cell = max.y - min.y;
        const int count = static_cast<int>(std::ranges::count(has, true));
        const float gap = count > 1 ? (max.x - min.x - cell * static_cast<float>(count)) /
                                          static_cast<float>(count - 1)
                                    : 0;
        float x = min.x;
        for (std::size_t d = 0; d < has.size(); ++d) {
            if (!has[d]) {
                continue;
            }
            Pen cellPen(list, {x, min.y}, {x + cell, max.y}, look.alpha);
            ItemLook one = look;
            one.lit = lit[d];
            d == 0 ? Tv(cellPen, one) : d == 1 ? Keyboard(cellPen, one) : Knob(cellPen, one);
            x += cell + gap;
        }
        break;
    }
    case Item::Brief:
        Brief(p, look);
        break;
    }
}

}  // namespace bw
