#include "ui/board_view.hpp"

#include "board/geometry.hpp"
#include "ui/bench.hpp"
#include "ui/board_look.hpp"
#include "view/chip_body.hpp"
#include "view/flip_corner.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "board/copper.hpp"
#include "ui/keycap.hpp"
#include "view/digit_shape.hpp"

namespace bw {

namespace {

// Look and feel: first guesses.
constexpr ImU32 kBoardGreen = IM_COL32(18, 66, 38, 255);
constexpr ImU32 kFrontCopper = IM_COL32(200, 125, 60, 255);
constexpr ImU32 kBackCopper = IM_COL32(70, 120, 200, 255);
constexpr ImU32 kPadTin = IM_COL32(200, 200, 185, 255);
constexpr ImU32 kPadGlow =
    IM_COL32(205, 155, 30, 255);  // a pin on the hovered chip's wiring: dim gold
constexpr ImU32 kHole = IM_COL32(12, 12, 12, 255);
constexpr ImU32 kCourtyard = IM_COL32(190, 190, 190, 150);
constexpr ImU32 kReference = IM_COL32(230, 230, 230, 200);
constexpr ImU32 kPinOne = IM_COL32(230, 230, 230, 230);
constexpr ImU32 kHighlight = IM_COL32(255, 190, 0, 255);
// The power rails in the gold's place, so a glance tells VCC from GND, as a breadboard's red and
// blue rails: bright for the Net under the cursor, dim for a chip's power pins. The blue stays
// brighter than the back copper's.
constexpr ImU32 kVccLit = IM_COL32(255, 70, 40, 255);
constexpr ImU32 kVccGlow = IM_COL32(215, 55, 35, 255);
constexpr ImU32 kGndLit = IM_COL32(90, 190, 255, 255);
constexpr ImU32 kGndGlow = IM_COL32(70, 150, 215, 255);
// The selected part's box: white, apart from the gold wiring beside it.
constexpr ImU32 kSelection = IM_COL32(245, 245, 240, 255);
constexpr ImU32 kChipGold = IM_COL32(190, 140, 20, 255);  // a hovered chip's wiring (plain look)
constexpr float kPlainDimShare = 0.35f;  // other traces while a chip's wiring shows
constexpr ImU32 kDriverMark = IM_COL32(255, 255, 255, 255);
constexpr float kDriverMarkGap = 5.0f;  // px outside the pad, outside the hover ring
constexpr ImU32 kProbeRed = IM_COL32(220, 30, 30, 255);
constexpr ImU32 kSolder = IM_COL32(205, 205, 210, 255);
constexpr ImU32 kCutMark = IM_COL32(20, 20, 20, 255);
constexpr std::int64_t kCutHalf = 300'000;  // nm: half the X's arm
constexpr float kCutMinPx = 4;
constexpr std::int64_t kMarkSize = 3'000'000;  // nm across: a grease pencil's mark (P6h)
constexpr float kMarkMinPx = 10;
constexpr ImU32 kPencil = IM_COL32(205, 205, 200, 255);  // opaque: the discs overlap
constexpr ImU32 kWireInsulation = IM_COL32(120, 170, 255, 255);
constexpr std::int64_t kWireWidth = 300'000;
constexpr std::int64_t kBridgeWidth = 600'000;  // nm
constexpr std::int64_t kBlobRadius = 600'000;   // covers the gap of a cracked 0.2 mm Trace
constexpr float kBlobMinPx = 3;
constexpr float kInactiveAlpha = 0.35f;
constexpr double kReferenceMm = 1.5;
constexpr float kReferenceMinPx = 10;
constexpr float kReferenceMaxPx = 28;
constexpr float kPartNameShare = 0.6f;
constexpr ImU32 kPartName = IM_COL32(200, 200, 200, 170);
constexpr float kPinNameMinPadPx = 10;
constexpr double kPinNameMm = 0.9;
constexpr float kPinNameMinPx = 10;
constexpr float kPinNameMaxPx = 16;
constexpr float kPinNameGapPx = 3;
constexpr ImU32 kPinPlate = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kPinName = IM_COL32(240, 240, 240, 255);
constexpr ImU32 kFlash = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kLedDark = IM_COL32(90, 20, 20, 255);
constexpr ImU32 kLedLit = IM_COL32(255, 40, 30, 255);
constexpr float kLedRadiusShare = 0.39f;  // of the courtyard's smaller side: 2.5 mm on LED_D5.0mm
constexpr float kLedHalo = 1.6f;
constexpr float kLedHaloAlpha = 0.35f;
constexpr ImU32 kDigitBody = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kDigitRim = IM_COL32(105, 105, 110, 255);  // a digit's grey plastic case (P6g)
constexpr ImU32 kDigitBevelLight = IM_COL32(150, 150, 155, 255);
constexpr ImU32 kDigitBevelDark = IM_COL32(60, 60, 64, 255);
constexpr float kDigitRimShare = 0.06f;  // of the courtyard's smaller side
constexpr ImU32 kSegmentDim = IM_COL32(70, 22, 22, 255);
constexpr ImU32 kButtonBody = IM_COL32(40, 40, 45, 255);
constexpr ImU32 kButtonCap = IM_COL32(170, 170, 175, 255);
constexpr ImU32 kButtonCapDown = IM_COL32(95, 95, 100, 255);
constexpr ImU32 kButtonRim = IM_COL32(230, 230, 235, 255);
constexpr float kButtonCapShare = 0.35f;  // of the courtyard's smaller side
constexpr float kButtonCapDownScale = 0.85f;
// The real look's tactile switch (P6g): a black 6 x 3.5 mm body between its pads, a round
// plunger.
constexpr ImU32 kTactBody = IM_COL32(22, 22, 24, 255);
constexpr ImU32 kTactEdge = IM_COL32(60, 60, 64, 255);
constexpr ImU32 kTactLeg = IM_COL32(190, 190, 195, 255);
constexpr ImU32 kTactShadow = IM_COL32(0, 0, 0, 90);
constexpr ImU32 kPlunger = IM_COL32(185, 185, 190, 255);
constexpr ImU32 kPlungerLight = IM_COL32(235, 235, 240, 200);
constexpr ImU32 kPlungerDown = IM_COL32(110, 110, 115, 255);
constexpr std::int64_t kTactInset = 250'000;  // the body's ends inside the pad centres
constexpr std::int64_t kTactHalf = 1'750'000;
constexpr std::int64_t kPlungerRadius = 1'100'000;
constexpr std::int64_t kPlungerDownRadius = 950'000;
constexpr std::int64_t kLegLength = 1'200'000;
constexpr std::int64_t kLegHalf = 400'000;
constexpr float kSwitchPrintMinPx = 6;
constexpr float kSwitchPrintMinPartPx = 16;               // a switch smaller on screen: no print
constexpr std::int64_t kLegendGap = 400'000;              // a legend under its part's courtyard
constexpr ImU32 kSlideBody = IM_COL32(150, 30, 30, 255);  // a red DIP switch body
constexpr ImU32 kSlideSlot = IM_COL32(45, 12, 12, 255);
constexpr ImU32 kSlideKnob = IM_COL32(235, 232, 222, 255);
constexpr ImU32 kSlideRidge = IM_COL32(150, 145, 135, 255);
constexpr ImU32 kSlideText = IM_COL32(245, 235, 225, 255);
constexpr float kSlideBodyShare = 0.78f;  // of the courtyard's height
constexpr double kPadReachPx = 12;
constexpr double kCopperReachPx = 10;
constexpr double kFocusPadMinPx = 14;
constexpr std::int64_t kFocusPadMargin = 4'000'000;  // nm around a focused pad
constexpr auto kFlashTime = std::chrono::seconds(1);

ImU32 WithAlpha(ImU32 colour, float alpha) {
    const auto a =
        static_cast<ImU32>(static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xFF) * alpha + 0.5f);
    return (colour & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

ImU32 Lerp(ImU32 from, ImU32 to, float t) {
    ImU32 result = 0;
    for (const int shift :
         {IM_COL32_R_SHIFT, IM_COL32_G_SHIFT, IM_COL32_B_SHIFT, IM_COL32_A_SHIFT}) {
        const auto a = static_cast<float>((from >> shift) & 0xFF);
        const auto b = static_cast<float>((to >> shift) & 0xFF);
        result |= static_cast<ImU32>(a + (b - a) * t + 0.5f) << shift;
    }
    return result;
}

// Index of the wire nearest `at` within `reach` nm.
std::optional<std::size_t> NearestWire(std::span<const PadJoin> wires, Point at, double reach) {
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < wires.size(); ++i) {
        const double d = SegmentDistance(at, wires[i].a, wires[i].b);
        if (d <= reach) {
            best = i;
            reach = d;
        }
    }
    return best;
}

// Board nm → absolute ImGui screen position.
class Mapper {
public:
    Mapper(const Viewport& viewport, ImVec2 origin, ImVec2 panel)
        : viewport_(viewport), origin_(origin), panel_(panel) {}

    ImVec2 operator()(Point p) const {
        const ScreenPoint s = viewport_.toScreen(p);
        return {origin_.x + static_cast<float>(s.x), origin_.y + static_cast<float>(s.y)};
    }
    float pixels(std::int64_t nm) const { return static_cast<float>(viewport_.toPixels(nm)); }
    double pixelsPerMm() const { return viewport_.pixelsPerMm(); }

    // Board box grown by `margin` nm, off the panel?
    bool offPanel(Point a, Point b, std::int64_t margin) const {
        const ImVec2 p = (*this)({std::min(a.x, b.x) - margin, std::min(a.y, b.y) - margin});
        const ImVec2 q = (*this)({std::max(a.x, b.x) + margin, std::max(a.y, b.y) + margin});
        return q.x < origin_.x || q.y < origin_.y || p.x > origin_.x + panel_.x ||
               p.y > origin_.y + panel_.y;
    }

private:
    const Viewport& viewport_;
    ImVec2 origin_;
    ImVec2 panel_;
};

void DrawSegment(ImDrawList& list, ImVec2 a, ImVec2 b, float width, ImU32 colour) {
    list.AddLine(a, b, colour, width);
    list.AddCircleFilled(a, width / 2, colour);
    list.AddCircleFilled(b, width / 2, colour);
}

void DrawTrace(ImDrawList& list, const Mapper& map, const Trace& trace, ImU32 colour,
               std::span<const CrackMark> cracks) {
    if (map.offPanel(trace.start, trace.end, trace.width) && !trace.mid) {
        return;
    }
    const float width = std::max(1.0f, map.pixels(trace.width));
    if (!trace.mid) {
        const auto [atStart, atEnd] = TraceCracks(trace, cracks);
        DrawStraightTrace(list, map(trace.start), map(trace.end), width, colour,
                          atStart ? std::optional(map(*atStart)) : std::nullopt,
                          atEnd ? std::optional(map(*atEnd)) : std::nullopt,
                          map.pixels(kCrackWidth));
        return;
    }
    const auto points = ArcPolyline(trace.start, *trace.mid, trace.end);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        DrawSegment(list, map(points[i]), map(points[i + 1]), width, colour);
    }
}

// Pad-local offset (x, y) nm, rotated like the pad, → screen.
ImVec2 PadPoint(const Mapper& map, const Pad& pad, double x, double y) {
    return map(RotatedOffset(pad.position, x, y, pad.angle));
}

void DrawPad(ImDrawList& list, const Mapper& map, const Pad& pad, ImU32 colour) {
    const double w = static_cast<double>(pad.size.x);
    const double h = static_cast<double>(pad.size.y);
    const std::int64_t reach = std::max(pad.size.x, pad.size.y);
    if (map.offPanel(pad.position, pad.position, reach)) {
        return;
    }
    switch (pad.shape) {
    case PadShape::Circle:
        list.AddCircleFilled(map(pad.position), map.pixels(pad.size.x) / 2, colour);
        break;
    case PadShape::Oval: {
        const double half = std::abs(w - h) / 2;
        const ImVec2 a = w > h ? PadPoint(map, pad, -half, 0) : PadPoint(map, pad, 0, -half);
        const ImVec2 b = w > h ? PadPoint(map, pad, half, 0) : PadPoint(map, pad, 0, half);
        DrawSegment(list, a, b, map.pixels(std::min(pad.size.x, pad.size.y)), colour);
        break;
    }
    case PadShape::Rectangle:
        list.AddQuadFilled(PadPoint(map, pad, -w / 2, -h / 2), PadPoint(map, pad, w / 2, -h / 2),
                           PadPoint(map, pad, w / 2, h / 2), PadPoint(map, pad, -w / 2, h / 2),
                           colour);
        break;
    case PadShape::RoundedRectangle: {
        const double r = static_cast<double>(pad.cornerRadius);
        const float radius = map.pixels(pad.cornerRadius);
        // Screen angles grow clockwise (y down); the pad turns counter-clockwise.
        const float turn = static_cast<float>(-pad.angle * std::numbers::pi / 180);
        const float quarter = std::numbers::pi_v<float> / 2;
        const double x = w / 2 - r;
        const double y = h / 2 - r;
        list.PathArcTo(PadPoint(map, pad, x, y), radius, turn, turn + quarter);
        list.PathArcTo(PadPoint(map, pad, -x, y), radius, turn + quarter, turn + 2 * quarter);
        list.PathArcTo(PadPoint(map, pad, -x, -y), radius, turn + 2 * quarter, turn + 3 * quarter);
        list.PathArcTo(PadPoint(map, pad, x, -y), radius, turn + 3 * quarter, turn + 4 * quarter);
        list.PathFillConvex(colour);
        break;
    }
    }
    if (pad.drill > 0) {
        list.AddCircleFilled(map(pad.position), map.pixels(pad.drill) / 2, kHole);
    }
}

void DrawVia(ImDrawList& list, const Mapper& map, const Via& via, ImU32 colour) {
    if (map.offPanel(via.position, via.position, via.diameter)) {
        return;
    }
    list.AddCircleFilled(map(via.position), std::max(1.0f, map.pixels(via.diameter) / 2), colour);
    list.AddCircleFilled(map(via.position), map.pixels(via.drill) / 2, kHole);
}

ImVec2 Centre(std::span<const ImVec2> points) {
    ImVec2 centre{0, 0};
    for (const ImVec2 p : points) {
        centre = {centre.x + p.x, centre.y + p.y};
    }
    const auto n = static_cast<float>(points.size());
    return {centre.x / n, centre.y / n};
}

void DrawCourtyard(ImDrawList& list, const Mapper& map, const BoardComponent& component,
                   std::span<const Point> courtyard, std::string_view partName, bool lit,
                   bool onKeycap) {
    std::vector<ImVec2> points;
    for (const Point p : courtyard) {
        points.push_back(map(p));
    }
    const ImVec2 centre = Centre(points);
    list.AddPolyline(points.data(), static_cast<int>(points.size()), lit ? kSelection : kCourtyard,
                     ImDrawFlags_Closed, lit ? 2.0f : 1.0f);

    const float size = std::clamp(static_cast<float>(kReferenceMm * map.pixelsPerMm()),
                                  kReferenceMinPx, kReferenceMaxPx);
    ImFont* font = ImGui::GetFont();
    const char* text = component.reference.c_str();
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    const float partSize = size * kPartNameShare;
    const std::string part(partName);
    const ImVec2 partTextSize = font->CalcTextSizeA(partSize, FLT_MAX, 0, part.c_str());
    float width = 0;
    for (const ImVec2 p : points) {
        width = std::max(width, std::abs(p.x - centre.x) * 2);
    }
    const bool showPart = !onKeycap && !part.empty() && part != component.reference &&
                          partTextSize.x <= width && textSize.x <= width;
    const float top = centre.y - (textSize.y + (showPart ? partTextSize.y : 0)) / 2;
    if (onKeycap) {
        // A key's legend fills its cap: the reference goes small in the top left corner.
        const float small = size * kPartNameShare;
        const auto [minX, maxX] = std::ranges::minmax(points, {}, &ImVec2::x);
        const auto [minY, maxY] = std::ranges::minmax(points, {}, &ImVec2::y);
        list.AddText(font, small, {minX.x + small * 0.4f, minY.y + small * 0.3f}, kReference, text);
    } else {
        list.AddText(font, size, {centre.x - textSize.x / 2, top}, kReference, text);
    }
    if (showPart) {
        list.AddText(font, partSize, {centre.x - partTextSize.x / 2, top + textSize.y}, kPartName,
                     part.c_str());
    }

    for (const Pad& pad : component.pads) {
        if (pad.number != "1") {
            continue;
        }
        const ImVec2 at = map(pad.position);
        const auto distance = [&](ImVec2 p) {
            return (p.x - at.x) * (p.x - at.x) + (p.y - at.y) * (p.y - at.y);
        };
        const ImVec2 corner = *std::ranges::min_element(
            points, [&](ImVec2 a, ImVec2 b) { return distance(a) < distance(b); });
        list.AddCircleFilled(corner, std::max(2.0f, map.pixels(300'000)), kPinOne);
    }
}

// Pad's half extent along screen x and y, rotation included.
ImVec2 PadHalf(const Mapper& map, const Pad& pad) {
    const double radians = pad.angle * std::numbers::pi / 180;
    const double c = std::abs(std::cos(radians));
    const double s = std::abs(std::sin(radians));
    const double w = static_cast<double>(pad.size.x) / 2;
    const double h = static_cast<double>(pad.size.y) / 2;
    return {map.pixels(std::llround(c * w + s * h)), map.pixels(std::llround(s * w + c * h))};
}

// Pin names beside the pads, away from the courtyard centre, on dark plates.
void DrawPinNames(ImDrawList& list, const Mapper& map, const BoardScene& scene,
                  std::size_t component) {
    const BoardComponent& c = scene.board().components[component];
    float smallest = FLT_MAX;
    for (const Pad& pad : c.pads) {
        smallest = std::min(smallest, map.pixels(std::min(pad.size.x, pad.size.y)));
    }
    if (c.pads.empty() || smallest < kPinNameMinPadPx) {
        return;
    }
    std::vector<ImVec2> points;
    for (const Point p : scene.courtyard(component)) {
        points.push_back(map(p));
    }
    const ImVec2 centre = Centre(points);
    ImVec2 extent{0, 0};
    for (const ImVec2 p : points) {
        extent = {std::max(extent.x, std::abs(p.x - centre.x)),
                  std::max(extent.y, std::abs(p.y - centre.y))};
    }
    const bool across = extent.y >= extent.x;  // tall chip: names left/right of the pads
    const float size = std::clamp(static_cast<float>(kPinNameMm * map.pixelsPerMm()), kPinNameMinPx,
                                  kPinNameMaxPx);
    ImFont* font = ImGui::GetFont();
    for (std::size_t p = 0; p < c.pads.size(); ++p) {
        const std::string name(scene.pinName(component, p));
        if (name.empty()) {
            continue;
        }
        const ImVec2 at = map(c.pads[p].position);
        const ImVec2 half = PadHalf(map, c.pads[p]);
        const ImVec2 text = font->CalcTextSizeA(size, FLT_MAX, 0, name.c_str());
        const float dx = centre.x - at.x;
        const float dy = centre.y - at.y;
        ImVec2 corner;
        if (across) {
            const float start = half.x + kPinNameGapPx;
            corner = {dx >= 0 ? at.x - start - text.x : at.x + start, at.y - text.y / 2};
        } else {
            const float start = half.y + kPinNameGapPx;
            corner = {at.x - text.x / 2, dy >= 0 ? at.y - start - text.y : at.y + start};
        }
        list.AddRectFilled({corner.x - 2, corner.y - 2},
                           {corner.x + text.x + 2, corner.y + text.y + 2}, kPinPlate, 2.0f);
        list.AddText(font, size, corner, kPinName, name.c_str());
    }
}

// How bright a digit's segment looks for the share of time it is lit. Multiplexed digits are
// driven harder while selected, and the eye's response is close to a cube root (Stevens): a
// digit of six lit a sixth of the time reads clearly, not at a sixth. Round LEDs stay linear, so
// a half-lit address LED (Heartbeat) still looks half lit.
float Perceived(float brightness) { return std::cbrt(std::clamp(brightness, 0.0f, 1.0f)); }

// A slide switch in a red DIP body: a dark slot along its long side, the ridged knob at the
// right (ON) end while on, else at the left. The plain look prints ON by the right end and the
// legend under the slot, inside the body; the real look prints them on the silkscreen
// (`font` null: the caller does).
void DrawSlide(ImDrawList& list, ImFont* font, ImVec2 a, ImVec2 b, const std::string& legend,
               bool on) {
    const float w = b.x - a.x;
    const float h = b.y - a.y;
    const float inset = h * (1 - kSlideBodyShare) / 2;
    const ImVec2 bodyA{a.x + inset, a.y + inset};
    const ImVec2 bodyB{b.x - inset, b.y - inset};
    list.AddRectFilled(bodyA, bodyB, kSlideBody, h * 0.05f);
    const float slotH = (bodyB.y - bodyA.y) * 0.34f;
    const float slotW = (bodyB.x - bodyA.x) * 0.5f;
    const ImVec2 slotA{a.x + (w - slotW) / 2, a.y + h * 0.5f - slotH / 2 - h * 0.08f};
    const ImVec2 slotB{slotA.x + slotW, slotA.y + slotH};
    list.AddRectFilled(slotA, slotB, kSlideSlot, slotH * 0.15f);
    const float knobW = slotW * 0.55f;
    const float gap = slotH * 0.12f;
    const float knobX = on ? slotB.x - gap - knobW : slotA.x + gap;
    list.AddRectFilled({knobX, slotA.y + gap}, {knobX + knobW, slotB.y - gap}, kSlideKnob,
                       slotH * 0.1f);
    for (const float r : {0.3f, 0.5f, 0.7f}) {  // the ridges across it
        const float x = knobX + knobW * r;
        list.AddLine({x, slotA.y + gap * 2}, {x, slotB.y - gap * 2}, kSlideRidge,
                     std::max(1.0f, knobW * 0.05f));
    }
    if (font == nullptr) {
        return;
    }
    const float small = slotH * 0.55f;
    const ImVec2 onSize = font->CalcTextSizeA(small, FLT_MAX, 0, "ON");
    list.AddText(font, small,
                 {slotB.x + (bodyB.x - slotB.x - onSize.x) / 2, slotA.y + (slotH - onSize.y) / 2},
                 kSlideText, "ON");
    if (!legend.empty()) {
        const float size = slotH * 0.75f;
        const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0, legend.c_str());
        const float y = slotB.y + (bodyB.y - slotB.y - t.y) / 2;
        list.AddText(font, size, {a.x + (w - t.x) / 2, y}, kSlideText, legend.c_str());
    }
}

// A switch's print on the silkscreen: the references' size, but never below 6 px while the
// switch itself is large enough to use (the player needs to tell START from RESET even where the
// references are too small to print); 0 below that.
float SwitchPrintPx(const LookMap& look, ImVec2 a, ImVec2 b) {
    if (std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) < kSwitchPrintMinPartPx) {
        return 0;
    }
    return std::clamp(look.pixels(kSilkTextNm), kSwitchPrintMinPx, 22.0f);
}

// The silkscreen's ON beside a slide switch's right end, centred on its slot (P6g).
void DrawSlideOn(ImDrawList& list, const LookMap& look, ImVec2 a, ImVec2 b) {
    const float size = SwitchPrintPx(look, a, b);
    if (size == 0) {
        return;
    }
    ImFont* font = ImGui::GetFont();
    const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0, "ON");
    const float h = b.y - a.y;
    const float slotMid = a.y + h * 0.5f - h * 0.08f;
    list.AddText(font, size, {b.x + size * 0.3f, slotMid - t.y / 2}, kSilk, "ON");
}

// A switch's legend on the silkscreen (P6g): centred under its courtyard, or, when under would
// leave the Board, a line above its reference. Which, decided in Board nm with the print's
// real height, so the zoom does not move it.
void DrawSilkLegend(ImDrawList& list, const LookMap& look, const BoardScene& scene,
                    std::size_t component, const std::string& legend) {
    const auto courtyard = scene.courtyard(component);
    if (legend.empty() || courtyard.empty()) {
        return;
    }
    const auto [minX, maxX] = std::ranges::minmax(courtyard, {}, &Point::x);
    const auto [minY, maxY] = std::ranges::minmax(courtyard, {}, &Point::y);
    const float size = SwitchPrintPx(look, look({minX.x, minY.y}), look({maxX.x, maxY.y}));
    if (size == 0) {
        return;
    }
    const std::int64_t centreX = (minX.x + maxX.x) / 2;
    ImFont* font = ImGui::GetFont();
    const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0, legend.c_str());
    const bool under = maxY.y + kLegendGap + kSilkTextNm <= scene.bounds().max.y;
    ImVec2 at;
    if (under) {
        const ImVec2 top = look({centreX, maxY.y + kLegendGap});
        at = {top.x - t.x / 2, top.y};
    } else {
        // DrawUseReferences prints the reference on the courtyard's top, 0.25 mm inside it (when
        // large enough to print): the legend above it.
        const float reference = SilkTextPx(look);
        const float referenceHigh =
            reference > 0 ? font->CalcTextSizeA(reference, FLT_MAX, 0, "SW").y + 1 : 0;
        const ImVec2 top = look({centreX, minY.y + 250'000});
        at = {top.x - t.x / 2, top.y - referenceHigh - t.y};
    }
    list.AddText(font, size, at, kSilk, legend.c_str());
}

// A tactile switch in the real look (P6g): from its two pad centres, its legs, a black body
// between them and a round plunger, light while up, darker and smaller held.
void DrawTactile(ImDrawList& list, const LookMap& look, const BoardComponent& component,
                 bool pressed) {
    if (component.pads.size() != 2) {
        return;
    }
    const Point p1 = component.pads[0].position;
    const Point p2 = component.pads[1].position;
    const double length =
        std::hypot(static_cast<double>(p2.x - p1.x), static_cast<double>(p2.y - p1.y));
    if (length <= 2.0 * kTactInset) {
        return;
    }
    const double ux = static_cast<double>(p2.x - p1.x) / length;
    const double uy = static_cast<double>(p2.y - p1.y) / length;
    // Board nm, from p1 along the pads' line (s) and across it (t).
    const auto at = [&](double s, double t) {
        return look({p1.x + std::llround(ux * s - uy * t), p1.y + std::llround(uy * s + ux * t)});
    };
    const auto quad = [&](double s0, double s1, double half, ImU32 colour, ImVec2 lift) {
        const ImVec2 q[4] = {at(s0, -half), at(s1, -half), at(s1, half), at(s0, half)};
        list.AddQuadFilled({q[0].x + lift.x, q[0].y + lift.y}, {q[1].x + lift.x, q[1].y + lift.y},
                           {q[2].x + lift.x, q[2].y + lift.y}, {q[3].x + lift.x, q[3].y + lift.y},
                           colour);
    };
    const double inset = kTactInset;
    // The legs: from outside each pad's centre into the body.
    quad(-(kLegLength - inset - kLegHalf), inset + kLegHalf, kLegHalf, kTactLeg, {0, 0});
    quad(length - inset - kLegHalf, length + (kLegLength - inset - kLegHalf), kLegHalf, kTactLeg,
         {0, 0});
    const float lift = look.pixels(250'000);
    quad(inset, length - inset, kTactHalf, kTactShadow, {lift, lift});
    quad(inset, length - inset, kTactHalf, kTactBody, {0, 0});
    // The light edge on the long side higher on the screen.
    const ImVec2 sideA0 = at(inset, -kTactHalf), sideA1 = at(length - inset, -kTactHalf);
    const ImVec2 sideB0 = at(inset, kTactHalf), sideB1 = at(length - inset, kTactHalf);
    const bool aHigher = sideA0.y + sideA1.y <= sideB0.y + sideB1.y;
    list.AddLine(aHigher ? sideA0 : sideB0, aHigher ? sideA1 : sideB1, kTactEdge, 1.0f);
    const ImVec2 centre = at(length / 2, 0);
    if (pressed) {
        list.AddCircleFilled(centre, look.pixels(kPlungerDownRadius), kPlungerDown);
        return;
    }
    const float r = look.pixels(kPlungerRadius);
    list.AddCircleFilled(centre, r, kPlunger);
    list.AddCircleFilled({centre.x - r * 0.35f, centre.y - r * 0.35f}, r * 0.3f, kPlungerLight);
}

// A stable pseudo-random number in [-1, 1] from a mark's place and an index: the same every
// frame, different from mark to mark (the hand's tilt, the wax's uneven edge).
float Wobble(std::uint64_t seed, std::uint64_t index) {
    std::uint64_t h = (seed ^ (index * 0x9E3779B97F4A7C15ull)) * 0xBF58476D1CE4E5B9ull;
    h ^= h >> 31;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 29;
    return static_cast<float>(h & 0xFFFF) / 32767.5f - 1.0f;
}

// A wax stroke from `a` through the control point `c` to `b` (a quadratic curve), its width
// going from `w0` to `w1`: discs along it, so the ends are round and the edge a little uneven.
void WaxStroke(ImDrawList& list, ImVec2 a, ImVec2 c, ImVec2 b, float w0, float w1,
               std::uint64_t seed) {
    const float length = std::hypot(c.x - a.x, c.y - a.y) + std::hypot(b.x - c.x, b.y - c.y);
    const float step = std::max(0.75f, std::min(w0, w1) * 0.25f);
    const int steps = std::max(2, static_cast<int>(length / step));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float u = 1 - t;
        const ImVec2 at{u * u * a.x + 2 * u * t * c.x + t * t * b.x,
                        u * u * a.y + 2 * u * t * c.y + t * t * b.y};
        const float width = (w0 + (w1 - w0) * t) * (1 + 0.08f * Wobble(seed, i / 3));
        list.AddCircleFilled(at, std::max(1.0f, width / 2), kPencil, 12);
    }
}

// A grease pencil's mark of `size` px across, centred on `c`, drawn as a hand would: curved
// tapering strokes, tilted a little, from `seed` (its place) so it is the same every frame.
void DrawPencilMark(ImDrawList& list, ImVec2 c, float size, MarkShape shape, std::uint64_t seed) {
    const float tilt = 0.18f * Wobble(seed, 101);
    const float scale = size * (1 + 0.06f * Wobble(seed, 102));
    const float cosT = std::cos(tilt), sinT = std::sin(tilt);
    const auto at = [&](float x, float y) {  // unit coordinates, y down, about the centre
        return ImVec2{c.x + scale * (x * cosT - y * sinT), c.y + scale * (x * sinT + y * cosT)};
    };
    const auto width = [&](float share) { return std::max(2.0f, share * scale); };
    switch (shape) {
    case MarkShape::Dot:  // a short dab
        WaxStroke(list, at(-0.07f, -0.03f), at(0.0f, 0.04f), at(0.08f, -0.01f), width(0.38f),
                  width(0.34f), seed);
        break;
    case MarkShape::Check:
        WaxStroke(list, at(-0.42f, 0.0f), at(-0.3f, 0.2f), at(-0.12f, 0.34f), width(0.14f),
                  width(0.19f), seed);
        WaxStroke(list, at(-0.12f, 0.34f), at(0.06f, -0.04f), at(0.46f, -0.42f), width(0.19f),
                  width(0.08f), seed + 1);
        break;
    case MarkShape::Cross:
        WaxStroke(list, at(-0.4f, -0.38f), at(0.04f, -0.04f), at(0.42f, 0.4f), width(0.18f),
                  width(0.11f), seed);
        WaxStroke(list, at(0.38f, -0.41f), at(0.04f, 0.04f), at(-0.4f, 0.38f), width(0.17f),
                  width(0.1f), seed + 1);
        break;
    }
}
}  // namespace

void BoardView::draw(ImVec2 size) {
    // A still (P7b) lets the mouse through to the launcher's buttons under it.
    ImGui::BeginChild("board", size, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                          (still_ ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 panel = ImGui::GetContentRegionAvail();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    SharpFont(legendFont_);
    list.AddRectFilled(origin, {origin.x + panel.x, origin.y + panel.y}, kPanelBackground);
    if (!scene_) {
        const char* text = "no board";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        list.AddText({origin.x + (panel.x - textSize.x) / 2, origin.y + (panel.y - textSize.y) / 2},
                     IM_COL32_WHITE, text);
        ImGui::EndChild();
        return;
    }

    const ScreenPoint panelSize{panel.x, panel.y};
    if (panel.x > 0 && panel.y > 0) {
        if (still_ && (needFit_ || panel.x != panel_.x || panel.y != panel_.y)) {
            viewport_.fit(scene_->bounds(), panelSize);
            viewport_.frame(*still_, 1.0, 0);
            needFit_ = false;
        } else if (needFit_) {
            viewport_.fit(scene_->bounds(), panelSize);
            if (zoomAtStart_) {
                viewport_.frame(*zoomAtStart_, 1.0, 0);
                zoomAtStart_.reset();
            }
            needFit_ = false;
        } else if (panel.x != panel_.x || panel.y != panel_.y) {
            viewport_.resize(panelSize);
        }
        panel_ = panel;
    }
    if (!still_) {
        input(origin);
    }
    const std::optional<BoardTarget> outside = std::exchange(outside_, std::nullopt);
    if (!ImGui::IsWindowHovered() && outside) {
        hoverTarget_ = outside;
        if (outside->net) {
            highlight_ = outside->net;
        } else if (outside->pad && !outside->components.empty()) {
            highlight_ = scene_->padNet(outside->components.front(), *outside->pad);
        } else {
            highlight_.reset();
        }
    }
    // A Net link clicked: its Net stays lit while it flashes.
    if (!highlight_ && flash_ && flash_->target.net &&
        std::chrono::steady_clock::now() < flash_->until) {
        highlight_ = flash_->target.net;
    }

    const Mapper map(viewport_, origin, panel);
    const LookMap look{viewport_, origin};
    const Board& board = scene_->board();
    const bool real = !plainLook_;
    std::vector<ImVec2> outline;
    for (const Point p : scene_->outline()) {
        outline.push_back(map(p));
    }
    if (!outline.empty()) {
        ImVec2 corner = outline.front();
        for (const ImVec2 q : outline) {
            corner = {std::min(corner.x, q.x), std::min(corner.y, q.y)};
        }
        corner_ = corner;
    }
    const std::optional<NetId> lit = highlight_;
    // Gold, or the rail's own colour on VCC and GND (the builder names every power Net so).
    const auto railColour = [&](std::optional<NetId> net, bool bright) {
        const std::string_view name = net ? scene_->netName(*net) : std::string_view{};
        if (name == "VCC") {
            return bright ? kVccLit : kVccGlow;
        }
        if (name == "GND") {
            return bright ? kGndLit : kGndGlow;
        }
        return bright ? kHighlight : kPadGlow;
    };
    const ImU32 litGold = railColour(lit, true);
    const auto drawTraces = [&](Layer layer, float alpha, float litAlpha, bool litOnly) {
        const ImU32 colour = WithAlpha(layer == Layer::Front ? kFrontCopper : kBackCopper, alpha);
        const ImU32 litColour = WithAlpha(litGold, litAlpha);
        for (std::size_t t = 0; t < board.traces.size(); ++t) {
            const Trace& trace = board.traces[t];
            if (trace.layer == layer) {
                const bool on = lit && scene_->traceNet(t) == lit;
                if (on || !litOnly) {
                    DrawTrace(list, map, trace, on ? litColour : colour, cracks_);
                }
            }
        }
    };
    const Layer inactive = active_ == Layer::Front ? Layer::Back : Layer::Front;
    // The part under the cursor, while no single Net is: its Nets glow but the power rails, the
    // pins at both ends of them marked, and its own pins on a rail marked when connected (a
    // chip's VCC and GND, a tied input, a pull-up's VCC end) (P6 prototype).
    const std::optional<std::size_t> wired = !lit ? pointedComponent() : std::nullopt;
    const BoardScene::Wiring wiring = wired ? scene_->wiring(*wired) : BoardScene::Wiring{};
    const std::vector<NetId>& glowing = wiring.glowing;
    const std::vector<bool>& used = wiring.used;
    const std::span<const NetId> rails = scene_->rails();
    const auto drawPads = [&] {
        for (std::size_t k = 0; k < board.components.size(); ++k) {
            const auto& pads = board.components[k].pads;
            for (std::size_t p = 0; p < pads.size(); ++p) {
                const auto net = scene_->padNet(k, p);
                const bool on = lit && net == lit;
                const bool glows = net && std::ranges::contains(glowing, *net);
                const bool rail =
                    k == wired && net && std::ranges::contains(rails, *net) && used[p];
                DrawPad(list, map, pads[p],
                        on      ? litGold
                        : rail  ? railColour(net, false)
                        : glows ? kPadGlow
                                : kPadTin);
            }
        }
        for (std::size_t v = 0; v < board.vias.size(); ++v) {
            const bool on = lit && scene_->viaNet(v) == lit;
            if (on || !real) {
                DrawVia(list, map, board.vias[v], on ? litGold : kPadTin);
            }
        }
    };
    if (real) {
        // The Board as the real one; the Net under the cursor lit over the mask.
        DrawUseBase(list, look, *scene_, active_, glowing, cracks_);
        drawPads();
        std::vector<bool> own(board.components.size(), false);
        for (const LedTag& led : leds_) {
            own[led.component] = true;
        }
        for (const SwitchTag& button : switches_) {
            own[button.component] = true;
        }
        DrawUseParts(list, look, *scene_, own, pointedComponent(), 1.0f);
        // The lit wiring above the parts: a see-through body must not darken it.
        if (!glowing.empty()) {
            DrawUseBase(list, look, *scene_, active_, glowing, cracks_, true);
        }
        drawTraces(inactive, kInactiveAlpha, 1.0f, true);  // bright gold on both layers
        drawTraces(active_, 1.0f, 1.0f, true);
    } else {
        // The plain look: the same selections as the real one (P6), drawn on bare copper. The
        // other traces first (dimmed while a chip's wiring shows), then that wiring in dim gold,
        // then the Net under the cursor in bright gold, both on both layers.
        list.AddConcavePolyFilled(outline.data(), static_cast<int>(outline.size()), kBoardGreen);
        const float dim = glowing.empty() ? 1.0f : kPlainDimShare;
        for (const Layer layer : {inactive, active_}) {
            const float alpha = (layer == active_ ? 1.0f : kInactiveAlpha) * dim;
            const ImU32 colour =
                WithAlpha(layer == Layer::Front ? kFrontCopper : kBackCopper, alpha);
            for (std::size_t t = 0; t < board.traces.size(); ++t) {
                const auto net = scene_->traceNet(t);
                const bool selected = net && (net == lit || std::ranges::contains(glowing, *net));
                if (board.traces[t].layer == layer && !selected) {
                    DrawTrace(list, map, board.traces[t], colour, cracks_);
                }
            }
        }
        for (std::size_t t = 0; t < board.traces.size(); ++t) {
            const auto net = scene_->traceNet(t);
            if (net && std::ranges::contains(glowing, *net)) {
                DrawTrace(list, map, board.traces[t], kChipGold, cracks_);
            }
        }
        drawTraces(inactive, kInactiveAlpha, 1.0f, true);
        drawTraces(active_, 1.0f, 1.0f, true);
    }

    for (const LedTag& led : leds_) {
        if (led.shape != LampShape::Round) {
            continue;  // digit segments: below
        }
        const auto courtyard = scene_->courtyard(led.component);
        if (courtyard.empty()) {
            continue;
        }
        const auto [minX, maxX] = std::ranges::minmax(courtyard, {}, &Point::x);
        const auto [minY, maxY] = std::ranges::minmax(courtyard, {}, &Point::y);
        const ImVec2 centre = map({(minX.x + maxX.x) / 2, (minY.y + maxY.y) / 2});
        const float radius =
            kLedRadiusShare * map.pixels(std::min(maxX.x - minX.x, maxY.y - minY.y));
        const float glow = std::clamp(led.brightness, 0.0f, 1.0f);
        if (glow > 0) {
            list.AddCircleFilled(centre, radius * kLedHalo,
                                 WithAlpha(kLedLit, kLedHaloAlpha * glow));
        }
        list.AddCircleFilled(centre, radius, Lerp(kLedDark, kLedLit, glow));
        if (!plainLook_) {
            // A dome: its rim, and the light caught on its top left.
            list.AddCircle(centre, radius, Lerp(IM_COL32(60, 12, 12, 255), kLedLit, glow), 0,
                           std::max(1.0f, radius * 0.12f));
            list.AddCircleFilled({centre.x - radius * 0.35f, centre.y - radius * 0.35f},
                                 radius * 0.28f, IM_COL32(255, 255, 255, 70));
            list.AddCircleFilled({centre.x - radius * 0.42f, centre.y - radius * 0.42f},
                                 radius * 0.12f, IM_COL32(255, 255, 255, 140));
        }
    }

    std::optional<std::size_t> bodyDrawn;
    for (const LedTag& led : leds_) {
        if (led.shape == LampShape::Round) {
            continue;
        }
        const auto courtyard = scene_->courtyard(led.component);
        const auto& pads = board.components[led.component].pads;
        const auto pad1 = std::ranges::find(pads, "1", &Pad::number);
        if (pad1 == pads.end()) {
            continue;
        }
        const auto frame = DigitFrameOf(courtyard, pad1->position);
        if (!frame) {
            continue;
        }
        std::vector<ImVec2> points;
        if (bodyDrawn != led.component) {
            for (const Point p : courtyard) {
                points.push_back(map(p));
            }
            // The grey case, its black face inset, bevelled: lit along the top and left.
            list.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), kDigitRim);
            const auto [minX, maxX] = std::ranges::minmax(courtyard, {}, &Point::x);
            const auto [minY, maxY] = std::ranges::minmax(courtyard, {}, &Point::y);
            const auto inset = static_cast<std::int64_t>(
                kDigitRimShare * static_cast<float>(std::min(maxX.x - minX.x, maxY.y - minY.y)));
            const ImVec2 p = map({minX.x + inset, minY.y + inset});
            const ImVec2 q = map({maxX.x - inset, maxY.y - inset});
            const ImVec2 fa{std::min(p.x, q.x), std::min(p.y, q.y)};
            const ImVec2 fb{std::max(p.x, q.x), std::max(p.y, q.y)};
            list.AddRectFilled(fa, fb, kDigitBody);
            list.AddLine({fa.x, fa.y}, {fb.x, fa.y}, kDigitBevelLight);
            list.AddLine({fa.x, fa.y}, {fa.x, fb.y}, kDigitBevelLight);
            list.AddLine({fa.x, fb.y}, {fb.x, fb.y}, kDigitBevelDark);
            list.AddLine({fb.x, fa.y}, {fb.x, fb.y}, kDigitBevelDark);
            bodyDrawn = led.component;
        }
        points.clear();
        for (const Point p : LampOutline(*frame, led.shape)) {
            points.push_back(map(p));
        }
        const float glow = Perceived(led.brightness);
        list.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()),
                                 Lerp(kSegmentDim, kLedLit, glow));
    }

    for (const SwitchTag& button : switches_) {
        const auto courtyard = scene_->courtyard(button.component);
        if (courtyard.empty()) {
            continue;
        }
        const auto [minX, maxX] = std::ranges::minmax(courtyard, {}, &Point::x);
        const auto [minY, maxY] = std::ranges::minmax(courtyard, {}, &Point::y);
        const ImVec2 min = map({minX.x, minY.y});
        const ImVec2 max = map({maxX.x, maxY.y});
        if (button.kind == SwitchKind::Slide) {
            const ImVec2 a{std::min(min.x, max.x), std::min(min.y, max.y)};
            const ImVec2 b{std::max(min.x, max.x), std::max(min.y, max.y)};
            DrawSlide(list, real ? nullptr : legendFont_, a, b, button.legend, button.pressed);
            if (real) {
                DrawSlideOn(list, look, a, b);
                DrawSilkLegend(list, look, *scene_, button.component, button.legend);
            }
            continue;
        }
        if (button.kind == SwitchKind::Key && legendFont_ != nullptr) {
            // The cap covers the courtyard only, so the copper between keys stays in view.
            const ImVec2 a{std::min(min.x, max.x), std::min(min.y, max.y)};
            const ImVec2 b{std::max(min.x, max.x), std::max(min.y, max.y)};
            const KeyCap cap{0, 0, 1, button.legend, "", CapKind::Character, 0};
            DrawKeyCap(list, legendFont_, cap, a, b, std::min(b.x - a.x, b.y - a.y),
                       button.pressed);
            continue;
        }
        if (real) {
            DrawTactile(list, look, board.components[button.component], button.pressed);
            DrawSilkLegend(list, look, *scene_, button.component, button.legend);
            continue;
        }
        list.AddRectFilled({std::min(min.x, max.x), std::min(min.y, max.y)},
                           {std::max(min.x, max.x), std::max(min.y, max.y)}, kButtonBody, 2.0f);
        const ImVec2 centre = map({(minX.x + maxX.x) / 2, (minY.y + maxY.y) / 2});
        const float radius =
            kButtonCapShare * map.pixels(std::min(maxX.x - minX.x, maxY.y - minY.y));
        if (button.pressed) {
            list.AddCircleFilled(centre, radius * kButtonCapDownScale, kButtonCapDown);
        } else {
            list.AddCircleFilled(centre, radius, kButtonCap);
            list.AddCircle(centre, radius, kButtonRim, 0, 1.5f);
        }
    }

    if (!real) {
        drawPads();
    }
    const float bridgeWidth = std::max(1.0f, map.pixels(kBridgeWidth));
    for (const PadJoin& bridge : bridges_) {
        list.AddLine(map(bridge.a), map(bridge.b),
                     WithAlpha(kPadTin, active_ == Layer::Back ? 1.0f : kInactiveAlpha),
                     bridgeWidth);
    }
    for (const SolderBlob& blob : blobs_) {
        const float alpha = blob.layer == active_ ? 1.0f : kInactiveAlpha;
        list.AddCircleFilled(map(blob.at), std::max(kBlobMinPx, map.pixels(kBlobRadius)),
                             WithAlpha(kSolder, alpha));
    }
    for (const CrackMark& cut : cuts_) {
        const ImU32 colour = WithAlpha(kCutMark, cut.layer == active_ ? 1.0f : kInactiveAlpha);
        const ImVec2 c = map(cut.at);
        const float h = std::max(kCutMinPx, map.pixels(kCutHalf));
        list.AddLine({c.x - h, c.y - h}, {c.x + h, c.y + h}, colour, 2.0f);
        list.AddLine({c.x - h, c.y + h}, {c.x + h, c.y - h}, colour, 2.0f);
    }
    if (real) {
        DrawUseReferences(list, look, *scene_);
        if (selected_) {
            DrawUseSelection(list, look, *scene_, *selected_, kSelection);
        }
    }
    // Parts a manual link points at (one part or a run, P6i): outlined while it is hovered.
    if (hoverTarget_ && !hoverTarget_->pad) {
        for (const std::size_t k : hoverTarget_->components) {
            DrawUseSelection(list, look, *scene_, k, kHighlight);
        }
    }
    for (std::size_t k = 0; k < board.components.size() && !real; ++k) {
        const bool outlined = selected_ == k;  // the selection's box; a hover has none
        const bool keycap =
            legendFont_ != nullptr && std::ranges::any_of(switches_, [&](const SwitchTag& t) {
                return t.kind != SwitchKind::Button && t.component == k;
            });
        DrawCourtyard(list, map, board.components[k], scene_->courtyard(k), scene_->partName(k),
                      outlined, keycap);
    }
    if (hoverTarget_ && hoverTarget_->pad && !hoverTarget_->components.empty()) {
        const Pad& pad =
            board.components[hoverTarget_->components.front()].pads[*hoverTarget_->pad];
        const ImVec2 half = PadHalf(map, pad);
        list.AddCircle(map(pad.position), std::max(half.x, half.y) + 3, kHighlight, 0, 2.0f);
    }
    if (highlight_) {
        for (const ScenePad& driver : scene_->drivers(*highlight_)) {
            const Pad& pad = board.components[driver.component].pads[driver.pad];
            const ImVec2 half = PadHalf(map, pad);
            list.AddCircle(map(pad.position), std::max(half.x, half.y) + kDriverMarkGap,
                           kDriverMark, 0, 2.0f);
        }
    }
    if (flash_ && std::chrono::steady_clock::now() < flash_->until) {
        const double left =
            std::chrono::duration<double>(flash_->until - std::chrono::steady_clock::now()).count();
        const float alpha = static_cast<float>(0.5 + 0.5 * std::cos(left * 6 * std::numbers::pi));
        const BoardTarget& target = flash_->target;
        const auto ring = [&](const Pad& pad) {
            const ImVec2 half = PadHalf(map, pad);
            list.AddCircle(map(pad.position), std::max(half.x, half.y) + 4,
                           WithAlpha(kFlash, alpha), 0, 3.0f);
        };
        if (target.net) {
            for (std::size_t k = 0; k < board.components.size(); ++k) {
                for (std::size_t p = 0; p < board.components[k].pads.size(); ++p) {
                    if (scene_->padNet(k, p) == target.net) {
                        ring(board.components[k].pads[p]);
                    }
                }
            }
        }
        for (const std::size_t k : target.components) {
            const auto& pads = board.components[k].pads;
            for (std::size_t p = 0; p < pads.size(); ++p) {
                if (!target.pad || target.pad == p) {
                    ring(pads[p]);
                }
            }
        }
    } else {
        flash_.reset();
    }
    if (const auto component = pointedComponent()) {
        DrawPinNames(list, map, *scene_, *component);
    }
    const float wireWidth = std::max(2.0f, map.pixels(kWireWidth));
    const float wireBlob = std::max(kBlobMinPx, map.pixels(kBlobRadius / 2));
    for (const PadJoin& wire : bodgeWires_) {
        list.AddLine(map(wire.a), map(wire.b), kWireInsulation, wireWidth);
        list.AddCircleFilled(map(wire.a), wireBlob, kSolder);
        list.AddCircleFilled(map(wire.b), wireBlob, kSolder);
    }
    // The wire in hand: dragged from a pad, else started by a click.
    const std::optional<Point> band =
        wirePress_ && dragging_ ? std::optional(wirePress_->contact) : wireStart_;
    if (band && (ImGui::IsWindowHovered() || dragging_)) {
        list.AddLine(map(*band), ImGui::GetIO().MousePos, kWireInsulation, wireWidth);
        list.AddCircleFilled(map(*band), wireBlob, kSolder);
    }
    for (const PencilMark& mark : marks_) {
        const auto seed = (static_cast<std::uint64_t>(mark.at.x) * 0x9E3779B97F4A7C15ull) ^
                          static_cast<std::uint64_t>(mark.at.y);
        DrawPencilMark(list, map(mark.at), std::max(map.pixels(kMarkSize), kMarkMinPx), mark.shape,
                       seed);
    }
    if (tip_) {
        const float radius = std::max(4.0f, map.pixels(400'000));
        list.AddCircleFilled(map(*tip_), radius, kProbeRed);
        list.AddCircle(map(*tip_), radius, IM_COL32_WHITE, 0, 1.0f);
    }
    for (const ClipTag& tag : clips_) {
        const float side = std::max(12.0f, map.pixels(1'000'000));
        const ImVec2 pad = map(tag.at);
        const ImVec2 min{pad.x + 2, pad.y - 2 - side};
        const std::string number = std::to_string(tag.channel + 1);
        const float fontSize = std::min(ImGui::GetFontSize(), side);
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, number.c_str());
        const ImVec2 max{std::max(pad.x + 2 + side, min.x + text.x + 4), pad.y - 2};
        list.AddRectFilled(min, max, tag.colour, 3.0f);
        list.AddRect(min, max, IM_COL32_BLACK, 3.0f, 0, 1.0f);
        list.AddText(nullptr, fontSize,
                     {(min.x + max.x - text.x) / 2, (min.y + max.y - text.y) / 2}, IM_COL32_BLACK,
                     number.c_str());
    }
    for (const HookTag& tag : hooks_) {
        const float radius = std::max(7.0f, map.pixels(600'000));
        const ImVec2 pad = map(tag.at);
        const ImVec2 centre{pad.x - 2 - radius, pad.y - 2 - radius};
        list.AddCircleFilled(centre, radius, kHookColours[tag.channel]);
        list.AddCircle(centre, radius, IM_COL32_BLACK, 0, 1.0f);
        const char digit[2] = {static_cast<char>('1' + tag.channel), '\0'};
        const float fontSize = std::min(ImGui::GetFontSize(), radius * 2);
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, digit);
        list.AddText(nullptr, fontSize, {centre.x - text.x / 2, centre.y - text.y / 2},
                     IM_COL32_BLACK, digit);
    }
    if (refusal_ && std::chrono::steady_clock::now() < refusal_->until) {
        const ImVec2 text = ImGui::CalcTextSize(refusal_->reason.c_str());
        const ImVec2 min{refusal_->at.x + 14, refusal_->at.y + 14};
        const ImVec2 max{min.x + text.x + 12, min.y + text.y + 8};
        list.AddRectFilled(min, max, IM_COL32(60, 10, 10, 235), 4.0f);
        list.AddRect(min, max, IM_COL32(255, 90, 90, 255), 4.0f);
        list.AddText({min.x + 6, min.y + 4}, IM_COL32(255, 220, 220, 255),
                     refusal_->reason.c_str());
    } else {
        refusal_.reset();
    }
    for (const LeadTag& tag : leads_) {
        const ImU32 fill = tag.red ? kProbeRed : IM_COL32(20, 20, 20, 255);
        const ImVec2 point = map(tag.at);
        const float ring = std::max(4.0f, map.pixels(350'000));
        list.AddCircle(point, ring, fill, 0, 2.5f);
        const float side = std::max(12.0f, map.pixels(1'000'000));
        const ImVec2 corner{point.x - ring - 10, point.y + ring + 10};  // down-left: Hooks up-left
        list.AddLine(point, corner, fill, 2.0f);
        const ImVec2 min{corner.x - side, corner.y};
        const ImVec2 max{corner.x, corner.y + side};
        list.AddRectFilled(min, max, fill, 4.0f);
        list.AddRect(min, max, IM_COL32_WHITE, 4.0f, 0, 1.0f);
        const char* sign = tag.red ? "+" : "-";
        const float fontSize = std::min(ImGui::GetFontSize(), side);
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, sign);
        list.AddText(nullptr, fontSize,
                     {(min.x + max.x - text.x) / 2, (min.y + max.y - text.y) / 2}, IM_COL32_WHITE,
                     sign);
    }
    if (!still_) {
        // The flip corner (P6o), last: a control on the Board. The corner turned over, showing the
        // other side's copper; a fold line and its shadow along the diagonal.
        const Bounds b = scene_->bounds();
        const std::int64_t side = FlipCornerSide(viewport_.pixelsPerMm());
        const Layer other = active_ == Layer::Front ? Layer::Back : Layer::Front;
        const ImU32 back =
            plainLook_ ? (other == Layer::Front ? kFrontCopper : kBackCopper) : TraceColour(other);
        const ImVec2 left = map({b.max.x - side, b.max.y});
        const ImVec2 up = map({b.max.x, b.max.y - side});
        const ImVec2 corner = map(b.max);
        const ImVec2 inward{-2.5f, -2.5f};
        list.AddLine({left.x + inward.x, left.y + inward.y}, {up.x + inward.x, up.y + inward.y},
                     IM_COL32(0, 0, 0, 110), 4.0f);
        list.AddTriangleFilled(left, up, corner, back);
        list.AddLine(left, up, IM_COL32(255, 255, 255, 150), 1.5f);
    }
    ImGui::EndChild();
}

PickReach BoardView::reach() const {
    const double nmPerPixel = 1e6 / viewport_.pixelsPerMm();
    // A chip's body over the copper under it, its whole wiring shown at once (the user,
    // 2026-11-22), but with Option held, or with a tool that works on copper in hand: the
    // cutters and the iron reach the Trace under it, the hover showing what the click will cut
    // or mend (2026-11-24).
    const bool copperTool = owner_ == Owner::Cutters || owner_ == Owner::Iron;
    return {std::llround(kPadReachPx * nmPerPixel), std::llround(kCopperReachPx * nmPerPixel),
            !ImGui::GetIO().KeyAlt && !copperTool};
}

std::optional<std::size_t> BoardView::wireAt(Point at) const {
    return NearestWire(bodgeWires_, at, static_cast<double>(reach().copper));
}

std::optional<std::size_t> BoardView::bridgeAt(Point at) const {
    return NearestWire(bridges_, at, static_cast<double>(reach().copper));
}

void BoardView::input(ImVec2 origin) {
    highlight_.reset();
    hoverTarget_.reset();
    hovered_.reset();
    const ImGuiIO& io = ImGui::GetIO();
    const ScreenPoint mouse{io.MousePos.x - origin.x, io.MousePos.y - origin.y};
    const bool hoveredPanel = ImGui::IsWindowHovered();

    if (pressed_) {
        if (!dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            dragging_ = true;
            if (!wirePress_) {
                const ImVec2 total = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                viewport_.pan({total.x, total.y});
            }
        } else if (dragging_ && !wirePress_) {
            viewport_.pan({io.MouseDelta.x, io.MouseDelta.y});
        }
    }
    if (hoveredPanel && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0) ||
                         ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0))) {
        viewport_.pan({io.MouseDelta.x, io.MouseDelta.y});
    }

    const Point at = viewport_.toBoard(mouse);
    // The flip corner (P6o): a control on the Board, whatever the tool.
    const bool onCorner =
        OnFlipCorner(at, scene_->bounds(), FlipCornerSide(viewport_.pixelsPerMm()));
    // A memory chip's window (P6p): a control too, where it is drawn (the real look); the corner
    // wins.
    const std::optional<std::size_t> onWindow =
        plainLook_ || onCorner ? std::nullopt : MemoryWindowAt(*scene_, at);
    std::optional<Pick> picked;
    if (hoveredPanel || pressed_) {
        picked = scene_->pick(at, active_, reach());
    }
    if (pressed_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (!dragging_ && cornerPressed_) {
            if (onCorner) {
                toggleLayer();
            }
        } else if (!dragging_ && windowPressed_) {
            if (onWindow == windowPressed_) {
                contentsClick_ = windowPressed_;
            }
        } else if (!dragging_ || wirePress_) {
            std::optional<std::size_t> component;
            if (picked &&
                (picked->hit.kind == HitKind::Pad || picked->hit.kind == HitKind::Component)) {
                component = picked->hit.component;
            } else {
                component = scene_->componentAt(at);
            }
            click_ = BoardClick{at,
                                active_,
                                picked,
                                component,
                                io.KeyShift,
                                false,
                                dragging_ ? wirePress_ : std::nullopt};
        }
        pressed_ = false;
        wirePress_.reset();
        cornerPressed_ = false;
        windowPressed_.reset();
        dragging_ = false;
    }
    if (held_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        held_.reset();
    }
    if (!hoveredPanel) {
        return;
    }
    const auto buttonAt = [&]() -> std::optional<std::size_t> {
        const auto body = scene_->bodyAt(at);
        // Not under the Swap tool: a click on a switch body swaps it (P5i); nor the pencil: it
        // marks it (P6h); nor on the flip corner (P6o).
        if (body && !onCorner && owner_ != Owner::Puller && owner_ != Owner::Pencil &&
            std::ranges::any_of(switches_,
                                [&](const SwitchTag& b) { return b.component == *body; })) {
            return body;
        }
        return std::nullopt;
    };
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] <
            io.MouseDragThreshold * io.MouseDragThreshold) {
        latchToggle_ = buttonAt();
        if (!onCorner) {
            rightClick_ = at;
        }
    }
    if (io.MouseWheel != 0) {
        viewport_.zoomAt(mouse, io.MouseWheel);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        held_ = buttonAt();
        cornerPressed_ = onCorner;
        windowPressed_ = onWindow;
        if (held_ && std::ranges::any_of(switches_, [&](const SwitchTag& t) {
                return t.component == *held_ && t.kind == SwitchKind::Slide;
            })) {
            latchToggle_ = held_;  // a slide moves at the click and is not held
            held_.reset();
            pressed_ = false;
        } else {
            pressed_ = !held_;
        }
        dragging_ = false;
        const bool onPad = picked && picked->hit.kind == HitKind::Pad;
        wirePress_ = pressed_ && owner_ == Owner::Wire && onPad && !onCorner && !onWindow
                         ? picked
                         : std::nullopt;
    }
    // Not under the pencil: two quick clicks change a mark from dot to cross.
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !picked && !onCorner && !onWindow &&
        owner_ != Owner::Pencil) {
        fit();
    }
    if (dragging_) {
        return;
    }
    if (onCorner) {
        ImGui::SetTooltip("Flip the Board");
        return;
    }
    if (picked) {
        const Hit& hit = picked->hit;
        highlight_ = scene_->netOf(hit);
        if (hit.kind == HitKind::Pad) {
            hoverTarget_ = BoardTarget{{hit.component}, hit.index, std::nullopt};
            hovered_ = hit.component;
        } else if (hit.kind == HitKind::Component) {
            hovered_ = hit.component;
        }
        std::string text =
            onWindow   ? "Read " + scene_->board().components[*onWindow].reference + "'s contents"
            : tooltip_ ? tooltip_(*scene_, hit)
                       : scene_->hoverText(hit);
        // A chip's body over copper: how to reach it.
        if (!onWindow && !text.empty() && hit.kind == HitKind::Component) {
            PickReach copper = reach();
            copper.bodies = false;
            const auto under = scene_->pick(at, active_, copper);
            if (under && (under->hit.kind == HitKind::Trace || under->hit.kind == HitKind::Via)) {
                text += "\nOption: the copper under it";
            }
        }
        // A refusal is drawn where the tooltip would go: it wins while it shows.
        const bool refusing = refusal_ && std::chrono::steady_clock::now() < refusal_->until;
        if (!text.empty() && !refusing) {
            ImGui::SetTooltip("%s", text.c_str());
        }
    }
    if (!hovered_) {
        hovered_ = scene_->componentAt(at);
    }
}

void BoardView::refuse(std::string reason) {
    refusal_ = Refusal{std::move(reason), ImGui::GetIO().MousePos,
                       std::chrono::steady_clock::now() + std::chrono::milliseconds(1500)};
}

std::optional<BoardClick> BoardView::takeClick() { return std::exchange(click_, std::nullopt); }

std::optional<std::size_t> BoardView::takeContentsClick() {
    return std::exchange(contentsClick_, std::nullopt);
}

std::optional<Point> BoardView::takeRightClick() {
    return std::exchange(rightClick_, std::nullopt);
}

std::int64_t BoardView::markReach() const {
    const double nmPerPixel = 1e6 / viewport_.pixelsPerMm();
    return std::max<std::int64_t>(kMarkSize / 2, std::llround(kMarkMinPx / 2 * nmPerPixel));
}

std::optional<std::size_t> BoardView::takeLatchToggle() {
    return std::exchange(latchToggle_, std::nullopt);
}

void BoardView::focus(BoardTarget target) {
    if (!scene_) {
        return;
    }
    const auto box = scene_->boundsOf(target, kFocusPadMargin);
    if (!box) {
        return;
    }
    // One part: never so small its pads cannot be told apart.
    double minPixelsPerMm = 0;
    if (target.components.size() == 1 && !target.net) {
        const std::int64_t smallest = scene_->smallestPad(target.components.front());
        minPixelsPerMm = smallest > 0 ? kFocusPadMinPx / (static_cast<double>(smallest) / 1e6) : 0;
    }
    needFit_ = false;
    viewport_.frame(*box, target.pad ? 0.3 : 0.5, minPixelsPerMm);
    flash_ = Flash{std::move(target), std::chrono::steady_clock::now() + kFlashTime};
}

void BoardView::route(const TargetHits& hits) {
    if (hits.hovered) {
        hoverFromOutside(hits.hovered);
    }
    if (!hits.clicked || !scene_) {
        return;
    }
    const BoardTarget& target = *hits.clicked;
    const ImGuiIO& io = ImGui::GetIO();
    switch (RouteLink(owner_, target, io.KeyCtrl || io.KeySuper,
                      io.MouseClickedLastCount[ImGuiMouseButton_Left] >= 2)) {
    case LinkAction::Measure: {
        const std::size_t k = target.components.front();
        const Point at = scene_->board().components[k].pads[*target.pad].position;
        click_ = BoardClick{at, active_,     Pick{{HitKind::Pad, k, *target.pad}, active_, at},
                            k,  io.KeyShift, true};
        flash_ = Flash{target, std::chrono::steady_clock::now() + kFlashTime};
        break;
    }
    case LinkAction::FrameAndSelect:
        select(target.components.front());
        focus(target);
        break;
    case LinkAction::Frame:  // a run or a Net selects nothing; a double-click's second neither
        focus(target);
        break;
    }
}

void BoardView::setProbeTip(std::optional<Point> tip) { tip_ = tip; }

}  // namespace bw
