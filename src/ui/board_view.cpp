#include "ui/board_view.hpp"

#include "ui/board_look.hpp"

#include <algorithm>
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
constexpr ImU32 kPanelBackground = IM_COL32(20, 20, 24, 255);
constexpr ImU32 kBoardGreen = IM_COL32(18, 66, 38, 255);
constexpr ImU32 kFrontCopper = IM_COL32(200, 125, 60, 255);
constexpr ImU32 kBackCopper = IM_COL32(70, 120, 200, 255);
constexpr ImU32 kPadTin = IM_COL32(200, 200, 185, 255);
constexpr ImU32 kPadGlow = IM_COL32(150, 255, 175, 255);  // a pin on the hovered chip's wiring
constexpr ImU32 kHole = IM_COL32(12, 12, 12, 255);
constexpr ImU32 kCourtyard = IM_COL32(190, 190, 190, 150);
constexpr ImU32 kReference = IM_COL32(230, 230, 230, 200);
constexpr ImU32 kPinOne = IM_COL32(230, 230, 230, 230);
constexpr ImU32 kHighlight = IM_COL32(255, 190, 0, 255);
constexpr ImU32 kDriverMark = IM_COL32(255, 255, 255, 255);
constexpr float kDriverMarkGap = 5.0f;  // px outside the pad, outside the hover ring
constexpr ImU32 kProbeRed = IM_COL32(220, 30, 30, 255);
constexpr ImU32 kSolder = IM_COL32(205, 205, 210, 255);
constexpr ImU32 kCutMark = IM_COL32(20, 20, 20, 255);
constexpr std::int64_t kCutHalf = 300'000;  // nm: half the X's arm
constexpr float kCutMinPx = 4;
constexpr ImU32 kWireInsulation = IM_COL32(120, 170, 255, 255);
constexpr std::int64_t kWireWidth = 300'000;
constexpr std::int64_t kBridgeWidth = 600'000;  // nm
constexpr std::int64_t kBlobRadius = 600'000;   // covers the gap of a cracked 0.2 mm Trace
constexpr float kBlobMinPx = 3;
constexpr float kInactiveAlpha = 0.35f;
constexpr float kInactiveHighlightAlpha = 0.6f;
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
constexpr ImU32 kSegmentDim = IM_COL32(70, 22, 22, 255);
constexpr ImU32 kButtonBody = IM_COL32(40, 40, 45, 255);
constexpr ImU32 kButtonCap = IM_COL32(170, 170, 175, 255);
constexpr ImU32 kButtonCapDown = IM_COL32(95, 95, 100, 255);
constexpr ImU32 kButtonRim = IM_COL32(230, 230, 235, 255);
constexpr float kButtonCapShare = 0.35f;  // of the courtyard's smaller side
constexpr float kButtonCapDownScale = 0.85f;
constexpr ImU32 kSlideBody = IM_COL32(150, 30, 30, 255);  // a red DIP switch body
constexpr ImU32 kSlideSlot = IM_COL32(45, 12, 12, 255);
constexpr ImU32 kSlideKnob = IM_COL32(235, 232, 222, 255);
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

double SegmentDistance(Point p, Point a, Point b) {
    const double abx = static_cast<double>(b.x - a.x);
    const double aby = static_cast<double>(b.y - a.y);
    const double apx = static_cast<double>(p.x - a.x);
    const double apy = static_cast<double>(p.y - a.y);
    const double length2 = abx * abx + aby * aby;
    const double t = length2 == 0 ? 0 : std::clamp((apx * abx + apy * aby) / length2, 0.0, 1.0);
    return std::hypot(apx - abx * t, apy - aby * t);
}

// Index of the wire nearest `at` within `reach` nm.
std::optional<std::size_t> NearestWire(std::span<const BodgeWire> wires, Point at, double reach) {
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

void DrawTrace(ImDrawList& list, const Mapper& map, const Trace& trace, ImU32 colour) {
    if (map.offPanel(trace.start, trace.end, trace.width) && !trace.mid) {
        return;
    }
    const float width = std::max(1.0f, map.pixels(trace.width));
    if (!trace.mid) {
        DrawSegment(list, map(trace.start), map(trace.end), width, colour);
        return;
    }
    const auto points = ArcPolyline(trace.start, *trace.mid, trace.end);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        DrawSegment(list, map(points[i]), map(points[i + 1]), width, colour);
    }
}

// Pad-local offset (x, y) nm, rotated like the pad, → screen.
ImVec2 PadPoint(const Mapper& map, const Pad& pad, double x, double y) {
    const double radians = pad.angle * std::numbers::pi / 180;
    const double cos = std::cos(radians);
    const double sin = std::sin(radians);
    return map({pad.position.x + std::llround(x * cos + y * sin),
                pad.position.y + std::llround(-x * sin + y * cos)});
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
    list.AddPolyline(points.data(), static_cast<int>(points.size()), lit ? kHighlight : kCourtyard,
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

// A slide switch in a red DIP body: a dark slot along its long side, the knob at the right
// (ON) end while on, else at the left; ON printed by the right end, the legend under the slot.
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
    const float knobW = slotW * 0.45f;
    const float gap = slotH * 0.12f;
    const float knobX = on ? slotB.x - gap - knobW : slotA.x + gap;
    list.AddRectFilled({knobX, slotA.y + gap}, {knobX + knobW, slotB.y - gap}, kSlideKnob,
                       slotH * 0.1f);
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

}  // namespace

void BoardView::draw(ImVec2 size) {
    ImGui::BeginChild("board", size, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 panel = ImGui::GetContentRegionAvail();
    ImDrawList& list = *ImGui::GetWindowDrawList();
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
        if (needFit_) {
            viewport_.fit(scene_->bounds(), panelSize);
            needFit_ = false;
        } else if (panel.x != panel_.x || panel.y != panel_.y) {
            viewport_.resize(panelSize);
        }
        panel_ = panel;
    }
    keys();
    input(origin);
    const std::optional<BoardTarget> outside = std::exchange(outside_, std::nullopt);
    if (!ImGui::IsWindowHovered() && outside) {
        hoverTarget_ = outside;
        highlight_ =
            outside->pad ? scene_->padNet(outside->component, *outside->pad) : std::nullopt;
    }

    const Mapper map(viewport_, origin, panel);
    const LookMap look{viewport_, origin};
    const Board& board = scene_->board();
    const bool real = !plainLook_;
    std::vector<ImVec2> outline;
    for (const Point p : scene_->outline()) {
        outline.push_back(map(p));
    }
    const std::optional<NetId> lit = highlight_;
    const auto drawTraces = [&](Layer layer, float alpha, float litAlpha, bool litOnly) {
        const ImU32 colour = WithAlpha(layer == Layer::Front ? kFrontCopper : kBackCopper, alpha);
        const ImU32 litColour = WithAlpha(kHighlight, litAlpha);
        for (std::size_t t = 0; t < board.traces.size(); ++t) {
            const Trace& trace = board.traces[t];
            if (trace.layer == layer) {
                const bool on = lit && scene_->traceNet(t) == lit;
                if (on || !litOnly) {
                    DrawTrace(list, map, trace, on ? litColour : colour);
                }
            }
        }
    };
    const Layer inactive = active_ == Layer::Front ? Layer::Back : Layer::Front;
    // The chip under the cursor, while no single Net is: its Nets glow but its power rails, the
    // pins at both ends of them marked, and its own power pins marked when connected (P6
    // prototype).
    const std::optional<std::size_t> wired = real && !lit ? chip() : std::nullopt;
    std::vector<NetId> glowing;
    std::vector<bool> used;    // [pad] of `wired`: on a Net that reaches another pad (rails)
    std::vector<NetId> rails;  // Nets a power pin is on: VCC and GND, never glowing
    if (wired) {
        for (std::size_t k = 0; k < board.components.size(); ++k) {
            for (std::size_t q = 0; q < board.components[k].pads.size(); ++q) {
                const auto net = scene_->padNet(k, q);
                if (net && scene_->powerPin(k, q) && !std::ranges::contains(rails, *net)) {
                    rails.push_back(*net);
                }
            }
        }
        const auto& pads = board.components[*wired].pads;
        used.assign(pads.size(), false);
        for (std::size_t p = 0; p < pads.size(); ++p) {
            const auto net = scene_->padNet(*wired, p);
            if (!net) {
                continue;
            }
            for (std::size_t k = 0; k < board.components.size() && !used[p]; ++k) {
                for (std::size_t q = 0; q < board.components[k].pads.size(); ++q) {
                    if ((k != *wired || q != p) && scene_->padNet(k, q) == net) {
                        used[p] = true;
                        break;
                    }
                }
            }
            // A Net that reaches no other pad (a named spare output) is no wiring.
            if (used[p] && !std::ranges::contains(rails, *net) &&
                !std::ranges::contains(glowing, *net)) {
                glowing.push_back(*net);
            }
        }
    }
    const auto drawPads = [&] {
        for (std::size_t k = 0; k < board.components.size(); ++k) {
            const auto& pads = board.components[k].pads;
            for (std::size_t p = 0; p < pads.size(); ++p) {
                const auto net = scene_->padNet(k, p);
                const bool on = lit && net == lit;
                const bool marked = (net && std::ranges::contains(glowing, *net)) ||
                                    (k == wired && scene_->powerPin(k, p) && used[p]);
                DrawPad(list, map, pads[p], on ? kHighlight : marked ? kPadGlow : kPadTin);
            }
        }
        for (std::size_t v = 0; v < board.vias.size(); ++v) {
            const bool on = lit && scene_->viaNet(v) == lit;
            if (on || !real) {
                DrawVia(list, map, board.vias[v], on ? kHighlight : kPadTin);
            }
        }
    };
    if (real) {
        // The Board as the real one; the Net under the cursor lit over the mask.
        DrawUseBase(list, look, *scene_, active_, glowing);
        drawPads();
        std::vector<bool> own(board.components.size(), false);
        for (const LedTag& led : leds_) {
            own[led.component] = true;
        }
        for (const SwitchTag& button : switches_) {
            own[button.component] = true;
        }
        DrawUseParts(list, look, *scene_, own, chip(), 1.0f);
        // The lit wiring above the parts: a see-through body must not darken it.
        if (!glowing.empty()) {
            DrawUseBase(list, look, *scene_, active_, glowing, true);
        }
        drawTraces(inactive, kInactiveAlpha, kInactiveHighlightAlpha, true);
        drawTraces(active_, 1.0f, 1.0f, true);
    } else {
        list.AddConcavePolyFilled(outline.data(), static_cast<int>(outline.size()), kBoardGreen);
        drawTraces(inactive, kInactiveAlpha, kInactiveHighlightAlpha, false);
        drawTraces(active_, 1.0f, 1.0f, false);
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
            list.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), kDigitBody);
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
        if (button.look == SwitchLook::Slide) {
            DrawSlide(list, legendFont_, {std::min(min.x, max.x), std::min(min.y, max.y)},
                      {std::max(min.x, max.x), std::max(min.y, max.y)}, button.legend,
                      button.pressed);
            continue;
        }
        if (button.look == SwitchLook::Key && legendFont_ != nullptr) {
            // The cap covers the courtyard only, so the copper between keys stays in view.
            const ImVec2 a{std::min(min.x, max.x), std::min(min.y, max.y)};
            const ImVec2 b{std::max(min.x, max.x), std::max(min.y, max.y)};
            const KeyCap cap{0, 0, 1, button.legend, "", CapKind::Character, 0};
            DrawKeyCap(list, legendFont_, cap, a, b, std::min(b.x - a.x, b.y - a.y),
                       button.pressed);
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
    for (const BodgeWire& bridge : bridges_) {
        list.AddLine(map(bridge.a), map(bridge.b),
                     WithAlpha(kPadTin, active_ == Layer::Back ? 1.0f : kInactiveAlpha),
                     bridgeWidth);
    }
    for (const SolderBlob& blob : blobs_) {
        const float alpha = blob.layer == active_ ? 1.0f : kInactiveAlpha;
        list.AddCircleFilled(map(blob.at), std::max(kBlobMinPx, map.pixels(kBlobRadius)),
                             WithAlpha(kSolder, alpha));
    }
    for (const CutMark& cut : cuts_) {
        const ImU32 colour = WithAlpha(kCutMark, cut.layer == active_ ? 1.0f : kInactiveAlpha);
        const ImVec2 c = map(cut.at);
        const float h = std::max(kCutMinPx, map.pixels(kCutHalf));
        list.AddLine({c.x - h, c.y - h}, {c.x + h, c.y + h}, colour, 2.0f);
        list.AddLine({c.x - h, c.y + h}, {c.x + h, c.y - h}, colour, 2.0f);
    }
    if (real) {
        DrawUseReferences(list, look, *scene_);
        // The part under the cursor: its outline, as in the plain look.
        if (hoverTarget_ && !hoverTarget_->pad) {
            std::vector<ImVec2> points;
            for (const Point p : scene_->courtyard(hoverTarget_->component)) {
                points.push_back(map(p));
            }
            list.AddPolyline(points.data(), static_cast<int>(points.size()), kHighlight,
                             ImDrawFlags_Closed, 2.0f);
        }
    }
    for (std::size_t k = 0; k < board.components.size() && !real; ++k) {
        const bool outlined = hoverTarget_ && hoverTarget_->component == k && !hoverTarget_->pad;
        const bool keycap =
            legendFont_ != nullptr && std::ranges::any_of(switches_, [&](const SwitchTag& t) {
                return t.look != SwitchLook::Button && t.component == k;
            });
        DrawCourtyard(list, map, board.components[k], scene_->courtyard(k), scene_->partName(k),
                      outlined, keycap);
    }
    if (hoverTarget_ && hoverTarget_->pad) {
        const Pad& pad = board.components[hoverTarget_->component].pads[*hoverTarget_->pad];
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
        const auto& pads = board.components[flash_->target.component].pads;
        for (std::size_t p = 0; p < pads.size(); ++p) {
            if (!flash_->target.pad || flash_->target.pad == p) {
                const ImVec2 half = PadHalf(map, pads[p]);
                list.AddCircle(map(pads[p].position), std::max(half.x, half.y) + 4,
                               WithAlpha(kFlash, alpha), 0, 3.0f);
            }
        }
    } else {
        flash_.reset();
    }
    if (const auto component = chip()) {
        DrawPinNames(list, map, *scene_, *component);
    }
    const float wireWidth = std::max(2.0f, map.pixels(kWireWidth));
    const float wireBlob = std::max(kBlobMinPx, map.pixels(kBlobRadius / 2));
    for (const BodgeWire& wire : bodgeWires_) {
        list.AddLine(map(wire.a), map(wire.b), kWireInsulation, wireWidth);
        list.AddCircleFilled(map(wire.a), wireBlob, kSolder);
        list.AddCircleFilled(map(wire.b), wireBlob, kSolder);
    }
    if (wireStart_ && ImGui::IsWindowHovered()) {
        list.AddLine(map(*wireStart_), ImGui::GetIO().MousePos, kWireInsulation, wireWidth);
        list.AddCircleFilled(map(*wireStart_), wireBlob, kSolder);
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
    ImGui::EndChild();
}

void BoardView::keys() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        toggleLayer();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        fit();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        setTool(Tool::Probe);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        setTool(Tool::Swap);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_M, false)) {
        setTool(Tool::Mend);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        setTool(Tool::Cut);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        setTool(Tool::Wire);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        setTool(Tool::Resolder);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        setTool(Tool::Clip);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        setTool(Tool::Hook);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        setTool(Tool::Meter);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        escape_ = true;
    }
    const ScreenPoint centre{panel_.x / 2, panel_.y / 2};
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
        viewport_.zoomAt(centre, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
        viewport_.zoomAt(centre, -1);
    }
}

PickReach BoardView::reach() const {
    const double nmPerPixel = 1e6 / viewport_.pixelsPerMm();
    return {std::llround(kPadReachPx * nmPerPixel), std::llround(kCopperReachPx * nmPerPixel)};
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
            const ImVec2 total = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            viewport_.pan({total.x, total.y});
        } else if (dragging_) {
            viewport_.pan({io.MouseDelta.x, io.MouseDelta.y});
        }
    }
    if (hoveredPanel && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0) ||
                         ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0))) {
        viewport_.pan({io.MouseDelta.x, io.MouseDelta.y});
    }

    const Point at = viewport_.toBoard(mouse);
    std::optional<Pick> picked;
    if (hoveredPanel || pressed_) {
        picked = scene_->pick(at, active_, reach());
    }
    if (pressed_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (!dragging_) {
            std::optional<std::size_t> component;
            if (picked &&
                (picked->hit.kind == HitKind::Pad || picked->hit.kind == HitKind::Component)) {
                component = picked->hit.component;
            } else {
                component = scene_->componentAt(at);
            }
            click_ = BoardClick{at, active_, picked, component, io.KeyShift};
        }
        pressed_ = false;
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
        // Not under the Swap tool: a click on a switch body swaps it (P5i).
        if (body && tool_ != Tool::Swap && std::ranges::any_of(switches_, [&](const SwitchTag& b) {
                return b.component == *body;
            })) {
            return body;
        }
        return std::nullopt;
    };
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] <
            io.MouseDragThreshold * io.MouseDragThreshold) {
        latchToggle_ = buttonAt();
    }
    if (io.MouseWheel != 0) {
        viewport_.zoomAt(mouse, io.MouseWheel);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        held_ = buttonAt();
        if (held_ && std::ranges::any_of(switches_, [&](const SwitchTag& t) {
                return t.component == *held_ && t.look == SwitchLook::Slide;
            })) {
            latchToggle_ = held_;  // a slide moves at the click and is not held
            held_.reset();
            pressed_ = false;
        } else {
            pressed_ = !held_;
        }
        dragging_ = false;
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !picked) {
        fit();
    }
    if (dragging_) {
        return;
    }
    if (picked) {
        const Hit& hit = picked->hit;
        highlight_ = scene_->netOf(hit);
        if (hit.kind == HitKind::Pad) {
            hoverTarget_ = BoardTarget{hit.component, hit.index};
            hovered_ = hit.component;
        } else if (hit.kind == HitKind::Component) {
            hovered_ = hit.component;
        }
        const std::string text = tooltip_ ? tooltip_(*scene_, hit) : scene_->hoverText(hit);
        if (!text.empty()) {
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

std::optional<std::size_t> BoardView::takeLatchToggle() {
    return std::exchange(latchToggle_, std::nullopt);
}

bool BoardView::takeEscape() { return std::exchange(escape_, false); }

void BoardView::focus(BoardTarget target) {
    if (!scene_) {
        return;
    }
    const BoardComponent& component = scene_->board().components[target.component];
    Bounds box{};
    if (target.pad) {
        const Point p = component.pads[*target.pad].position;
        box = {{p.x - kFocusPadMargin, p.y - kFocusPadMargin},
               {p.x + kFocusPadMargin, p.y + kFocusPadMargin}};
    } else {
        const auto courtyard = scene_->courtyard(target.component);
        box = {courtyard[0], courtyard[0]};
        for (const Point p : courtyard) {
            box.min = {std::min(box.min.x, p.x), std::min(box.min.y, p.y)};
            box.max = {std::max(box.max.x, p.x), std::max(box.max.y, p.y)};
        }
    }
    std::int64_t smallest = 0;
    for (const Pad& pad : component.pads) {
        const std::int64_t side = std::min(pad.size.x, pad.size.y);
        smallest = smallest == 0 ? side : std::min(smallest, side);
    }
    const double minPixelsPerMm =
        smallest > 0 ? kFocusPadMinPx / (static_cast<double>(smallest) / 1e6) : 0;
    needFit_ = false;
    viewport_.frame(box, target.pad ? 0.3 : 0.5, minPixelsPerMm);
    flash_ = Flash{target, std::chrono::steady_clock::now() + kFlashTime};
}

void BoardView::route(const TargetHits& hits) {
    if (hits.hovered) {
        hoverFromOutside(hits.hovered);
    }
    if (hits.clicked) {
        focus(*hits.clicked);
        select(hits.clicked->component);
    }
}

void BoardView::setProbeTip(std::optional<Point> tip) { tip_ = tip; }

}  // namespace bw
