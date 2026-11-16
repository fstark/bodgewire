#include "ui/board_look.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <functional>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "board/copper.hpp"

namespace bw {
namespace {

constexpr std::int64_t kMm = 1'000'000;
constexpr float kXray = 0.22f;            // a see-through part's opacity
constexpr float kSmallestPrintPx = 4.0f;  // print on a part, below which it is left out

// The Board's colours (P6, the user's pick of three): a dark mask; the copper under it, the front
// green and the back (seen through the board) teal, the active layer bright and on top, the
// other dim; the Nets of the chip the player works on in dim gold on both layers (every
// selection is gold: bright for the one Net under the cursor, dim for a chip's wiring).
struct Palette {
    ImU32 mask, frontActive, frontDim, backActive, backDim, frontGlow, backGlow, edge;
};
constexpr Palette kColours{IM_COL32(6, 52, 28, 255),    IM_COL32(90, 190, 70, 255),
                           IM_COL32(24, 84, 40, 255),   IM_COL32(40, 160, 190, 255),
                           IM_COL32(20, 88, 92, 255),   IM_COL32(190, 140, 20, 255),
                           IM_COL32(190, 140, 20, 255), IM_COL32(0, 24, 10, 255)};
constexpr const Palette& Colours() { return kColours; }

constexpr ImU32 kTin = IM_COL32(205, 205, 195, 255);
constexpr ImU32 kHole = IM_COL32(12, 12, 12, 255);
constexpr ImU32 kLeg = IM_COL32(185, 185, 180, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 90);
constexpr ImU32 kEpoxy = IM_COL32(26, 26, 29, 255);
constexpr ImU32 kEpoxyRim = IM_COL32(55, 55, 60, 255);
constexpr ImU32 kNotch = IM_COL32(10, 10, 12, 255);
constexpr ImU32 kPrint = IM_COL32(200, 200, 195, 230);  // the maker's print on a chip
constexpr ImU32 kCan = IM_COL32(178, 180, 184, 255);
constexpr ImU32 kCanRim = IM_COL32(225, 227, 230, 255);
constexpr ImU32 kCanPrint = IM_COL32(60, 60, 65, 255);
constexpr ImU32 kResistorBody = IM_COL32(214, 190, 150, 255);
constexpr ImU32 kTerminal = IM_COL32(40, 135, 80, 255);  // a green screw terminal
constexpr ImU32 kTerminalDark = IM_COL32(25, 95, 55, 255);
constexpr ImU32 kHousing = IM_COL32(32, 32, 34, 255);
constexpr ImU32 kSilk = IM_COL32(235, 235, 225, 215);

// Resistor colour code, digit → colour.
constexpr std::array<ImU32, 10> kBands = {
    IM_COL32(20, 20, 20, 255),    IM_COL32(120, 70, 30, 255),  IM_COL32(210, 30, 30, 255),
    IM_COL32(240, 130, 20, 255),  IM_COL32(240, 220, 30, 255), IM_COL32(40, 160, 60, 255),
    IM_COL32(40, 80, 210, 255),   IM_COL32(140, 60, 180, 255), IM_COL32(130, 130, 130, 255),
    IM_COL32(245, 245, 245, 255),
};
constexpr ImU32 kGold = IM_COL32(200, 160, 60, 255);
constexpr ImU32 kSilver = IM_COL32(190, 190, 195, 255);

struct Box {
    Point min, max;  // nm
    std::int64_t width() const { return max.x - min.x; }
    std::int64_t height() const { return max.y - min.y; }
    Point centre() const { return {(min.x + max.x) / 2, (min.y + max.y) / 2}; }
    Box grown(std::int64_t by) const {
        return {{min.x - by, min.y - by}, {max.x + by, max.y + by}};
    }
};

std::optional<Box> PadCentres(const BoardComponent& component) {
    if (component.pads.empty()) {
        return std::nullopt;
    }
    Box box{component.pads[0].position, component.pads[0].position};
    for (const Pad& pad : component.pads) {
        box.min = {std::min(box.min.x, pad.position.x), std::min(box.min.y, pad.position.y)};
        box.max = {std::max(box.max.x, pad.position.x), std::max(box.max.y, pad.position.y)};
    }
    return box;
}

const Pad* PadOne(const BoardComponent& component) {
    const auto it = std::ranges::find(component.pads, "1", &Pad::number);
    return it == component.pads.end() ? nullptr : &*it;
}

void Rect(ImDrawList& list, const LookMap& map, const Box& box, ImU32 colour, float roundMm = 0) {
    const ImVec2 a = map(box.min);
    const ImVec2 b = map(box.max);
    list.AddRectFilled({std::min(a.x, b.x), std::min(a.y, b.y)},
                       {std::max(a.x, b.x), std::max(a.y, b.y)}, colour,
                       map.pixels(static_cast<std::int64_t>(roundMm * kMm)));
}

void RectOutline(ImDrawList& list, const LookMap& map, const Box& box, ImU32 colour, float width,
                 float roundMm = 0) {
    const ImVec2 a = map(box.min);
    const ImVec2 b = map(box.max);
    list.AddRect({std::min(a.x, b.x), std::min(a.y, b.y)}, {std::max(a.x, b.x), std::max(a.y, b.y)},
                 colour, map.pixels(static_cast<std::int64_t>(roundMm * kMm)), 0, width);
}

// Text centred on `centre`, turned a quarter clockwise when `vertical` (read with the chip's
// pin 1 at the top, as a chip turned from pin 1 at the left).
void Print(ImDrawList& list, float size, ImVec2 centre, ImU32 colour, const std::string& text,
           bool vertical) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0, text.c_str());
    const int start = list.VtxBuffer.Size;
    list.AddText(font, size, {centre.x - extent.x / 2, centre.y - extent.y / 2}, colour,
                 text.c_str());
    if (!vertical) {
        return;
    }
    for (int i = start; i < list.VtxBuffer.Size; ++i) {
        ImVec2& p = list.VtxBuffer[i].pos;
        const ImVec2 d{p.x - centre.x, p.y - centre.y};
        p = {centre.x - d.y, centre.y + d.x};
    }
}

// A text size for `text` in a box `along` x `across` pixels: `preferred`, shrunk to fit; 0 when
// too small to read.
float FitText(const std::string& text, float preferred, float along, float across) {
    const float unit = ImGui::GetFont()->CalcTextSizeA(1.0f, FLT_MAX, 0, text.c_str()).x;
    float size = std::min(preferred, across);
    if (unit > 0) {
        size = std::min(size, along / unit);
    }
    return size >= kSmallestPrintPx ? size : 0;
}

struct ChipBody {
    Box box;
    bool horizontal;  // pad rows along x
};

// The epoxy body between a DIP's pad rows, past its end pins.
std::optional<ChipBody> BodyOf(const BoardComponent& component) {
    const auto pads = PadCentres(component);
    if (!pads) {
        return std::nullopt;
    }
    const bool horizontal = pads->width() >= pads->height();
    const std::int64_t inset = 650'000;       // body edge from a row's pad centres
    const std::int64_t overhang = 1'270'000;  // body past the end pins
    if (horizontal) {
        return ChipBody{{{pads->min.x - overhang, pads->min.y + inset},
                         {pads->max.x + overhang, pads->max.y - inset}},
                        true};
    }
    return ChipBody{{{pads->min.x + inset, pads->min.y - overhang},
                     {pads->max.x - inset, pads->max.y + overhang}},
                    false};
}

// An axial resistor's body: centred between its two pads, along them.
std::optional<Box> ResistorBodyOf(const BoardComponent& component) {
    if (component.pads.size() != 2) {
        return std::nullopt;
    }
    const Point a = component.pads[0].position;
    const Point b = component.pads[1].position;
    const double length =
        std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y));
    const std::int64_t half =
        static_cast<std::int64_t>(std::min(length - 2'400'000.0, 6'300'000.0) / 2);
    const std::int64_t width = 1'150'000;
    const Point c{(a.x + b.x) / 2, (a.y + b.y) / 2};
    if (std::abs(b.x - a.x) >= std::abs(b.y - a.y)) {
        return Box{{c.x - half, c.y - width}, {c.x + half, c.y + width}};
    }
    return Box{{c.x - width, c.y - half}, {c.x + width, c.y + half}};
}

// A dual-in-line chip: the epoxy body between the pad rows, legs into the pads, the notch at
// pin 1's end, the part number along the body.
void DrawChip(ImDrawList& list, const LookMap& map, const BoardScene& scene, std::size_t k) {
    const BoardComponent& component = scene.board().components[k];
    const Pad* one = PadOne(component);
    const auto shape = BodyOf(component);
    if (!shape || !one) {
        return;
    }
    const bool horizontal = shape->horizontal;
    const Box body = shape->box;
    const float legWidth = std::max(1.0f, map.pixels(500'000));
    for (const Pad& pad : component.pads) {
        Point inner = pad.position;
        if (horizontal) {
            inner.y = pad.position.y < body.centre().y ? body.min.y : body.max.y;
        } else {
            inner.x = pad.position.x < body.centre().x ? body.min.x : body.max.x;
        }
        list.AddLine(map(pad.position), map(inner), kLeg, legWidth);
        // The pin's end, down in the hole.
        const ImVec2 c = map(pad.position);
        const float tip = std::max(1.0f, map.pixels(330'000));
        list.AddRectFilled({c.x - tip, c.y - tip}, {c.x + tip, c.y + tip}, kLeg);
    }
    const std::int64_t lift = 300'000;
    Rect(list, map,
         {{body.min.x + lift, body.min.y + lift}, {body.max.x + lift, body.max.y + lift}}, kShadow,
         0.4f);
    Rect(list, map, body, kEpoxy, 0.4f);
    RectOutline(list, map, body, kEpoxyRim, 1.0f, 0.4f);

    // The notch: a half disc cut into the end nearest pin 1.
    const float notch = map.pixels(800'000);
    const float pi = std::numbers::pi_v<float>;
    ImVec2 notchAt;
    float from = 0;
    if (horizontal) {
        const bool left =
            std::abs(one->position.x - body.min.x) < std::abs(one->position.x - body.max.x);
        notchAt = map({left ? body.min.x : body.max.x, body.centre().y});
        from = left ? -pi / 2 : pi / 2;
    } else {
        const bool top =
            std::abs(one->position.y - body.min.y) < std::abs(one->position.y - body.max.y);
        notchAt = map({body.centre().x, top ? body.min.y : body.max.y});
        from = top ? 0 : pi;
    }
    list.PathArcTo(notchAt, notch, from, from + pi, 16);
    list.PathFillConvex(kNotch);

    // The part number along the body, as large as the body takes, readable when zoomed out.
    const ImVec2 a = map(body.min);
    const ImVec2 b = map(body.max);
    const float along = horizontal ? std::abs(b.x - a.x) : std::abs(b.y - a.y);
    const float across = horizontal ? std::abs(b.y - a.y) : std::abs(b.x - a.x);
    const std::string part(scene.partName(k));
    if (const float size = FitText(part, map.pixels(2'200'000), along * 0.85f, across * 0.6f)) {
        Print(list, size, map(body.centre()), kPrint, part, !horizontal);
    }
}

// A crystal oscillator in a metal can over its DIP footprint.
void DrawCan(ImDrawList& list, const LookMap& map, const BoardScene& scene, std::size_t k) {
    const auto pads = PadCentres(scene.board().components[k]);
    if (!pads) {
        return;
    }
    const Box can = pads->grown(900'000);
    Rect(list, map,
         {{can.min.x + 300'000, can.min.y + 300'000}, {can.max.x + 300'000, can.max.y + 300'000}},
         kShadow, 1.2f);
    Rect(list, map, can, kCan, 1.2f);
    RectOutline(list, map, can.grown(-350'000), kCanRim, std::max(1.0f, map.pixels(250'000)), 1.0f);
    // Its frequency, as a can is marked ("1.000MHz"); "OSC" if the netlist gives none.
    std::string text = std::string(scene.marking(k));
    if (text.ends_with("MHz")) {
        try {
            text = std::format("{:.3f}MHz", std::stod(text.substr(0, text.size() - 3)));
        } catch (const std::exception&) {
        }
    }
    if (text.empty()) {
        text = "OSC";
    }
    const ImVec2 a = map(can.min);
    const ImVec2 b = map(can.max);
    const bool upright = can.height() > can.width();
    const float along = upright ? std::abs(b.y - a.y) : std::abs(b.x - a.x);
    const float across = upright ? std::abs(b.x - a.x) : std::abs(b.y - a.y);
    if (const float size = FitText(text, map.pixels(2'000'000), along * 0.8f, across * 0.5f)) {
        Print(list, size, map(can.centre()), kCanPrint, text, upright);
    }
}

// Ohms from "4k7", "330", "1M"; nullopt if not a value.
std::optional<double> Ohms(std::string_view value) {
    std::string digits;
    double scale = 1;
    bool point = false;
    for (const char c : value) {
        if (c >= '0' && c <= '9') {
            digits += c;
        } else if ((c == 'k' || c == 'K' || c == 'M' || c == 'R' || c == '.') && !point) {
            scale = c == 'k' || c == 'K' ? 1e3 : c == 'M' ? 1e6 : 1;
            digits += '.';
            point = true;
        } else {
            return std::nullopt;
        }
    }
    if (digits.empty() || digits == ".") {
        return std::nullopt;
    }
    return std::stod(digits) * scale;
}

// Four bands: two digits, a multiplier, gold for 5 %.
std::array<ImU32, 4> Bands(double ohms) {
    const int exponent = static_cast<int>(std::floor(std::log10(ohms))) - 1;
    const int twoDigits = static_cast<int>(std::lround(ohms / std::pow(10.0, exponent)));
    const auto multiplier = [&]() -> ImU32 {
        if (exponent >= 0 && exponent <= 9) {
            return kBands[static_cast<std::size_t>(exponent)];
        }
        return exponent == -1 ? kGold : kSilver;
    }();
    return {kBands[static_cast<std::size_t>(twoDigits / 10 % 10)],
            kBands[static_cast<std::size_t>(twoDigits % 10)], multiplier, kGold};
}

// An axial resistor between its two pads: leads, a beige body, its colour bands.
void DrawResistor(ImDrawList& list, const LookMap& map, const BoardScene& scene, std::size_t k) {
    const auto& pads = scene.board().components[k].pads;
    if (pads.size() != 2) {
        return;
    }
    const ImVec2 a = map(pads[0].position);
    const ImVec2 b = map(pads[1].position);
    list.AddLine(a, b, kLeg, std::max(1.0f, map.pixels(500'000)));
    const float tip = std::max(1.0f, map.pixels(330'000));
    for (const ImVec2 c : {a, b}) {
        list.AddRectFilled({c.x - tip, c.y - tip}, {c.x + tip, c.y + tip}, kLeg);  // in the hole
    }
    const float length = std::hypot(b.x - a.x, b.y - a.y);
    if (length <= 0) {
        return;
    }
    const ImVec2 along{(b.x - a.x) / length, (b.y - a.y) / length};
    const ImVec2 across{-along.y, along.x};
    const ImVec2 mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
    const float bodyLength = std::min(length - map.pixels(2'400'000), map.pixels(6'300'000));
    const float half = map.pixels(1'150'000);
    if (bodyLength <= 0) {
        return;
    }
    const auto at = [&](float s, float t) {
        return ImVec2{mid.x + along.x * s + across.x * t, mid.y + along.y * s + across.y * t};
    };
    const float end = bodyLength / 2;
    list.AddLine(at(-end + half, map.pixels(250'000)), at(end - half, map.pixels(250'000)), kShadow,
                 half * 2);
    // A capsule: the body's straight part, round ends.
    list.AddLine(at(-end + half, 0), at(end - half, 0), kResistorBody, half * 2);
    list.AddCircleFilled(at(-end + half, 0), half, kResistorBody);
    list.AddCircleFilled(at(end - half, 0), half, kResistorBody);
    const auto ohms = Ohms(scene.value(k));
    if (!ohms || *ohms <= 0) {
        return;
    }
    const auto bands = Bands(*ohms);
    const float band = bodyLength * 0.09f;
    const std::array<float, 4> positions = {-0.30f, -0.14f, 0.02f, 0.30f};
    for (std::size_t i = 0; i < bands.size(); ++i) {
        const float s = positions[i] * bodyLength;
        list.AddQuadFilled(at(s, -half * 0.97f), at(s + band, -half * 0.97f),
                           at(s + band, half * 0.97f), at(s, half * 0.97f), bands[i]);
    }
}

// The power input: a green two-way screw terminal over its pads.
void DrawTerminal(ImDrawList& list, const LookMap& map, const BoardScene& scene, std::size_t k) {
    const BoardComponent& component = scene.board().components[k];
    const auto pads = PadCentres(component);
    if (!pads) {
        return;
    }
    const Box block = pads->grown(1'900'000);
    Rect(list, map,
         {{block.min.x + 300'000, block.min.y + 300'000},
          {block.max.x + 300'000, block.max.y + 300'000}},
         kShadow, 0.3f);
    Rect(list, map, block, kTerminal, 0.3f);
    RectOutline(list, map, block, kTerminalDark, 1.0f, 0.3f);
    const float screw = map.pixels(1'100'000);
    for (const Pad& pad : component.pads) {
        const ImVec2 c = map(pad.position);
        list.AddCircleFilled(c, screw, kLeg);
        list.AddCircle(c, screw, kTerminalDark, 0, 1.0f);
        list.AddLine({c.x - screw * 0.7f, c.y + screw * 0.7f},
                     {c.x + screw * 0.7f, c.y - screw * 0.7f}, kHole,
                     std::max(1.0f, screw * 0.25f));
    }
}

// Any other connector: a black header housing with its pins.
void DrawHeader(ImDrawList& list, const LookMap& map, const BoardScene& scene, std::size_t k) {
    const BoardComponent& component = scene.board().components[k];
    const auto pads = PadCentres(component);
    if (!pads) {
        return;
    }
    Rect(list, map, pads->grown(1'270'000), kHousing, 0.2f);
    const float pin = map.pixels(320'000);
    for (const Pad& pad : component.pads) {
        const ImVec2 c = map(pad.position);
        list.AddRectFilled({c.x - pin, c.y - pin}, {c.x + pin, c.y + pin}, kTin);
    }
}

enum class Kind { Chip, Can, Resistor, Terminal, Header, Other };

Kind KindOf(const BoardScene& scene, std::size_t k) {
    const std::string_view part = scene.partName(k);
    if (part == "OSC") {
        return Kind::Can;
    }
    if (part == "R") {
        return Kind::Resistor;
    }
    if (part == "POWER") {
        return Kind::Terminal;
    }
    if (scene.chip(k)) {
        return Kind::Chip;
    }
    if (scene.board().components[k].reference.starts_with("J")) {
        return Kind::Header;
    }
    return Kind::Other;
}

// The silkscreen box of a part: its pads' box grown to what the part covers.
std::optional<Box> SilkBox(const BoardScene& scene, std::size_t k) {
    const BoardComponent& component = scene.board().components[k];
    const auto pads = PadCentres(component);
    if (!pads) {
        return std::nullopt;
    }
    switch (KindOf(scene, k)) {
    case Kind::Chip:
        return BodyOf(component)->box.grown(400'000);
    case Kind::Can:
        return pads->grown(1'300'000);
    case Kind::Terminal:
        return pads->grown(2'300'000);
    case Kind::Header:
        return pads->grown(1'600'000);
    case Kind::Resistor:
        if (const auto body = ResistorBodyOf(component)) {
            return body->grown(350'000);
        }
        return pads->grown(1'000'000);
    case Kind::Other:
        break;
    }
    if (component.courtyard.empty()) {
        return pads->grown(1'000'000);
    }
    Box box{component.courtyard[0], component.courtyard[0]};
    for (const Point p : component.courtyard) {
        box.min = {std::min(box.min.x, p.x), std::min(box.min.y, p.y)};
        box.max = {std::max(box.max.x, p.x), std::max(box.max.y, p.y)};
    }
    return box.grown(-250'000);
}

}  // namespace

void DrawUseBase(ImDrawList& list, const LookMap& map, const BoardScene& scene, Layer active,
                 std::span<const NetId> glowing, bool glowOnly) {
    std::vector<ImVec2> outline;
    for (const Point p : scene.outline()) {
        outline.push_back(map(p));
    }
    if (!glowOnly) {
        list.AddConcavePolyFilled(outline.data(), static_cast<int>(outline.size()), Colours().mask);
    }
    const Board& board = scene.board();
    const auto draw = [&](Layer layer, ImU32 normal, ImU32 bright) {
        for (std::size_t t = 0; t < board.traces.size(); ++t) {
            const Trace& trace = board.traces[t];
            if (trace.layer != layer) {
                continue;
            }
            const auto net = scene.traceNet(t);
            const bool glows = net && std::ranges::find(glowing, *net) != glowing.end();
            if (glowOnly && !glows) {
                continue;
            }
            const ImU32 colour = glows ? bright : normal;
            const float width = std::max(1.0f, map.pixels(trace.width));
            const auto segment = [&](Point a, Point b) {
                list.AddLine(map(a), map(b), colour, width);
                list.AddCircleFilled(map(a), width / 2, colour);
                list.AddCircleFilled(map(b), width / 2, colour);
            };
            if (!trace.mid) {
                segment(trace.start, trace.end);
                continue;
            }
            const auto points = ArcPolyline(trace.start, *trace.mid, trace.end);
            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                segment(points[i], points[i + 1]);
            }
        }
    };
    const Layer other = active == Layer::Front ? Layer::Back : Layer::Front;
    // With a chip glowing, every other trace dims: its wiring is all that stands out.
    const auto colour = [&](Layer layer) {
        const bool bright = layer == active && glowing.empty();
        if (layer == Layer::Front) {
            return bright ? Colours().frontActive : Colours().frontDim;
        }
        return bright ? Colours().backActive : Colours().backDim;
    };
    const auto glowColour = [&](Layer layer) {
        return layer == Layer::Front ? Colours().frontGlow : Colours().backGlow;
    };
    draw(other, colour(other), glowColour(other));
    draw(active, colour(active), glowColour(active));
    if (glowOnly) {
        return;
    }
    for (const Via& via : board.vias) {
        list.AddCircleFilled(map(via.position), std::max(1.0f, map.pixels(via.diameter) / 2),
                             Colours().frontDim);
        list.AddCircleFilled(map(via.position), map.pixels(via.drill) / 2, kHole);
    }
    list.AddPolyline(outline.data(), static_cast<int>(outline.size()), Colours().edge,
                     ImDrawFlags_Closed, 3.0f);
}

void DrawUseParts(ImDrawList& list, const LookMap& map, const BoardScene& scene,
                  const std::vector<bool>& own, std::optional<std::size_t> xray, float alpha) {
    const auto& components = scene.board().components;
    const float silk = std::max(1.0f, map.pixels(150'000));
    for (std::size_t k = 0; k < components.size(); ++k) {
        const auto box = SilkBox(scene, k);
        if (!box) {
            continue;
        }
        if (scene.partName(k) == "LED") {
            // A round LED's outline: its circle.
            list.AddCircle(map(box->centre()),
                           map.pixels(std::min(box->width(), box->height()) / 2), kSilk, 0, silk);
            continue;
        }
        RectOutline(list, map, *box, kSilk, silk);
    }
    for (std::size_t k = 0; k < components.size(); ++k) {
        if (k < own.size() && own[k]) {
            continue;
        }
        const int start = list.VtxBuffer.Size;
        switch (KindOf(scene, k)) {
        case Kind::Chip:
            DrawChip(list, map, scene, k);
            break;
        case Kind::Can:
            DrawCan(list, map, scene, k);
            break;
        case Kind::Resistor:
            DrawResistor(list, map, scene, k);
            break;
        case Kind::Terminal:
            DrawTerminal(list, map, scene, k);
            break;
        case Kind::Header:
            DrawHeader(list, map, scene, k);
            break;
        case Kind::Other:
            break;
        }
        // See-through: the part the player works on, or every part from the back.
        const float fade = xray == k ? std::min(alpha, kXray) : alpha;
        if (fade < 1) {
            for (int v = start; v < list.VtxBuffer.Size; ++v) {
                ImU32& colour = list.VtxBuffer[v].col;
                const auto a = static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xFF) * fade;
                colour = (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
            }
        }
    }
}

void DrawUseReferences(ImDrawList& list, const LookMap& map, const BoardScene& scene) {
    const float size = std::clamp(map.pixels(1'400'000), 0.0f, 22.0f);
    if (size < 6) {
        return;
    }
    ImFont* font = ImGui::GetFont();
    const auto& components = scene.board().components;
    for (std::size_t k = 0; k < components.size(); ++k) {
        const auto box = SilkBox(scene, k);
        if (!box) {
            continue;
        }
        const std::string& text = components[k].reference;
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0, text.c_str());
        const ImVec2 top = map({box->centre().x, box->min.y});
        list.AddText(font, size, {top.x - extent.x / 2, top.y - extent.y - 1}, kSilk, text.c_str());
    }
}

}  // namespace bw
