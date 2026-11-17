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
constexpr ImU32 kFrontCopper = IM_COL32(60, 170, 80, 255);  // as the Board's layers
constexpr ImU32 kBackCopper = IM_COL32(40, 150, 160, 255);
constexpr ImU32 kBoardGreen = IM_COL32(20, 80, 40, 255);

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

void Run(Pen& p, const ItemLook& look) {
    p.box(8, 8, 48, 48, IM_COL32(60, 60, 64, 255), 8);
    if (look.lit) {  // running: pause bars
        p.box(19, 17, 25, 39, kWhite, 1);
        p.box(31, 17, 37, 39, kWhite, 1);
    } else {
        p.triangle(20, 16, 20, 40, 39, 28, kWhite);
    }
}

void Speed(Pen& p, const ItemLook& look) {
    p.text(15, 14, 11, look.maxSpeed ? kGrey : kWhite, "1×");
    p.text(40, 14, 11, look.maxSpeed ? kWhite : kGrey, "Max");
    p.box(8, 31, 48, 41, kBlack, 5);
    const float x = look.maxSpeed ? 38 : 18;
    p.box(x - 8, 28, x + 8, 44, kMetal, 3);
    p.line(x, 30, x, 42, kDarkMetal, 1.5f);
}

void Layer(Pen& p, const ItemLook& look) {
    const ImU32 front = look.front ? kFrontCopper : kBackCopper;
    const ImU32 back = look.front ? kBackCopper : kFrontCopper;
    p.box(18, 8, 48, 38, (back & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 110), 3);
    p.box(8, 18, 38, 48, front, 3);
    p.text(23, 33, 16, kWhite, look.front ? "F" : "B");
}

void Fit(Pen& p) {
    p.box(17, 19, 39, 37, kBoardGreen, 2);
    const float a = 8, b = 48, l = 9;
    p.line(a, a, a + l, a, kWhite, 2.5f);
    p.line(a, a, a, a + l, kWhite, 2.5f);
    p.line(b, a, b - l, a, kWhite, 2.5f);
    p.line(b, a, b, a + l, kWhite, 2.5f);
    p.line(a, b, a + l, b, kWhite, 2.5f);
    p.line(a, b, a, b - l, kWhite, 2.5f);
    p.line(b, b, b - l, b, kWhite, 2.5f);
    p.line(b, b, b, b - l, kWhite, 2.5f);
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

void Puller(Pen& p) {
    // A chip extractor: two prongs from a bow, hooked under a DIP's ends.
    p.line(14, 10, 42, 10, kMetal, 3);
    p.line(14, 10, 12, 36, kMetal, 3);
    p.line(42, 10, 44, 36, kMetal, 3);
    p.line(12, 36, 15, 39, kMetal, 3);
    p.line(44, 36, 41, 39, kMetal, 3);
    p.box(15, 34, 41, 44, kBlack, 1);
    for (float x = 18; x < 40; x += 5) {
        p.line(x, 44, x, 49, kMetal, 1.5f);
    }
}

void Iron(Pen& p) {
    // Handle bottom right, the shaft and copper tip to the upper left, a drop of solder.
    p.line(48, 48, 32, 32, IM_COL32(40, 70, 140, 255), 9);
    p.line(33, 33, 30, 30, kDarkMetal, 6);
    p.line(30, 30, 15, 15, kMetal, 3);
    p.line(15, 15, 9, 9, kCopper, 2.5f);
    p.disc(11, 18, 3, IM_COL32(200, 200, 210, 255));
}

void Cutters(Pen& p) {
    // Two jaws meeting at the top, a pivot, two red handles spreading down.
    p.quad(28, 6, 22, 14, 25, 26, 28, 24, kDarkMetal);
    p.quad(28, 6, 34, 14, 31, 26, 28, 24, kMetal);
    p.disc(28, 26, 3.5f, kDarkMetal);
    p.line(27, 28, 15, 50, kRed, 6);
    p.line(29, 28, 41, 50, kRed, 6);
}

void Wire(Pen& p) {
    // A coil of green wire, its end stripped to copper.
    for (int i = 0; i < 3; ++i) {
        const float x = 20 + static_cast<float>(i) * 5;
        p.ellipse(x, 28, 10, 16, IM_COL32(40, 170, 70, 255), 2.5f);
    }
    p.line(40, 32, 48, 44, IM_COL32(40, 170, 70, 255), 2.5f);
    p.line(48, 44, 51, 49, kCopper, 2);
}

void Tv(Pen& p, const ItemLook& look) {
    p.box(5, 8, 51, 44, IM_COL32(70, 62, 55, 255), 4);
    p.box(9, 12, 41, 40, look.lit ? kTvOn : kScreenOff, 6);
    p.disc(46, 18, 2.5f, kBlack);
    p.disc(46, 26, 2.5f, kBlack);
    p.line(16, 44, 12, 50, kDarkMetal, 2);
    p.line(40, 44, 44, 50, kDarkMetal, 2);
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

void Manual(Pen& p, const ItemLook& look) {
    if (look.lit) {  // open
        p.quad(4, 14, 27, 18, 27, 46, 4, 42, kWhite);
        p.quad(29, 18, 52, 14, 52, 42, 29, 46, kWhite);
        for (float y = 22; y < 40; y += 5) {
            p.line(8, y - 1, 23, y + 2, kGrey, 1);
            p.line(33, y + 2, 48, y - 1, kGrey, 1);
        }
    } else {
        p.box(12, 8, 44, 48, IM_COL32(40, 70, 130, 255), 2);
        p.box(12, 8, 17, 48, IM_COL32(25, 45, 90, 255), 1);
        p.box(22, 16, 38, 22, kWhite, 1);
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
    case Item::Run:
        Run(p, look);
        break;
    case Item::Speed:
        Speed(p, look);
        break;
    case Item::Layer:
        Layer(p, look);
        break;
    case Item::Fit:
        Fit(p);
        break;
    case Item::Probe:
        Probe(p, look);
        break;
    case Item::Puller:
        Puller(p);
        break;
    case Item::Iron:
        Iron(p);
        break;
    case Item::Cutters:
        Cutters(p);
        break;
    case Item::Wire:
        Wire(p);
        break;
    case Item::Tv:
        Tv(p, look);
        break;
    case Item::Keyboard:
        Keyboard(p, look);
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
    case Item::Knob:
        Knob(p, look);
        break;
    case Item::Brief:
        Brief(p, look);
        break;
    }
}

}  // namespace bw
