#include "ui/board_view.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "board/copper.hpp"

namespace bw {

namespace {

// Look and feel: first guesses.
constexpr ImU32 kPanelBackground = IM_COL32(20, 20, 24, 255);
constexpr ImU32 kBoardGreen = IM_COL32(18, 66, 38, 255);
constexpr ImU32 kFrontCopper = IM_COL32(200, 125, 60, 255);
constexpr ImU32 kBackCopper = IM_COL32(70, 120, 200, 255);
constexpr ImU32 kPadTin = IM_COL32(200, 200, 185, 255);
constexpr ImU32 kHole = IM_COL32(12, 12, 12, 255);
constexpr ImU32 kCourtyard = IM_COL32(190, 190, 190, 150);
constexpr ImU32 kReference = IM_COL32(230, 230, 230, 200);
constexpr ImU32 kPinOne = IM_COL32(230, 230, 230, 230);
constexpr ImU32 kHighlight = IM_COL32(255, 190, 0, 255);
constexpr ImU32 kProbeRed = IM_COL32(220, 30, 30, 255);
constexpr ImU32 kSolder = IM_COL32(205, 205, 210, 255);
constexpr std::int64_t kBlobRadius = 600'000;  // covers the gap of a cracked 0.2 mm Trace
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
                   std::span<const Point> courtyard, std::string_view partName, bool lit) {
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
    const bool showPart = !part.empty() && part != component.reference && partTextSize.x <= width &&
                          textSize.x <= width;
    const float top = centre.y - (textSize.y + (showPart ? partTextSize.y : 0)) / 2;
    list.AddText(font, size, {centre.x - textSize.x / 2, top}, kReference, text);
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
    const Board& board = scene_->board();
    std::vector<ImVec2> outline;
    for (const Point p : scene_->outline()) {
        outline.push_back(map(p));
    }
    list.AddConcavePolyFilled(outline.data(), static_cast<int>(outline.size()), kBoardGreen);

    const std::optional<NetId> lit = highlight_ ? highlight_ : net_;
    const auto drawTraces = [&](Layer layer, float alpha, float litAlpha) {
        const ImU32 colour = WithAlpha(layer == Layer::Front ? kFrontCopper : kBackCopper, alpha);
        const ImU32 litColour = WithAlpha(kHighlight, litAlpha);
        for (std::size_t t = 0; t < board.traces.size(); ++t) {
            const Trace& trace = board.traces[t];
            if (trace.layer == layer) {
                const bool on = lit && scene_->traceNet(t) == lit;
                DrawTrace(list, map, trace, on ? litColour : colour);
            }
        }
    };
    const Layer inactive = active_ == Layer::Front ? Layer::Back : Layer::Front;
    drawTraces(inactive, kInactiveAlpha, kInactiveHighlightAlpha);
    drawTraces(active_, 1.0f, 1.0f);

    for (std::size_t k = 0; k < board.components.size(); ++k) {
        const auto& pads = board.components[k].pads;
        for (std::size_t p = 0; p < pads.size(); ++p) {
            const bool on = lit && scene_->padNet(k, p) == lit;
            DrawPad(list, map, pads[p], on ? kHighlight : kPadTin);
        }
    }
    for (std::size_t v = 0; v < board.vias.size(); ++v) {
        const bool on = lit && scene_->viaNet(v) == lit;
        DrawVia(list, map, board.vias[v], on ? kHighlight : kPadTin);
    }
    for (const SolderBlob& blob : blobs_) {
        const float alpha = blob.layer == active_ ? 1.0f : kInactiveAlpha;
        list.AddCircleFilled(map(blob.at), std::max(kBlobMinPx, map.pixels(kBlobRadius)),
                             WithAlpha(kSolder, alpha));
    }
    for (std::size_t k = 0; k < board.components.size(); ++k) {
        const bool outlined = hoverTarget_ && hoverTarget_->component == k && !hoverTarget_->pad;
        DrawCourtyard(list, map, board.components[k], scene_->courtyard(k), scene_->partName(k),
                      outlined);
    }
    if (hoverTarget_ && hoverTarget_->pad) {
        const Pad& pad = board.components[hoverTarget_->component].pads[*hoverTarget_->pad];
        const ImVec2 half = PadHalf(map, pad);
        list.AddCircle(map(pad.position), std::max(half.x, half.y) + 3, kHighlight, 0, 2.0f);
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
    if (tip_) {
        const float radius = std::max(4.0f, map.pixels(400'000));
        list.AddCircleFilled(map(*tip_), radius, kProbeRed);
        list.AddCircle(map(*tip_), radius, IM_COL32_WHITE, 0, 1.0f);
    }
    for (const ClipTag& tag : clips_) {
        const float side = std::max(12.0f, map.pixels(1'000'000));
        const ImVec2 pad = map(tag.at);
        const ImVec2 min{pad.x + 2, pad.y - 2 - side};
        const ImVec2 max{pad.x + 2 + side, pad.y - 2};
        list.AddRectFilled(min, max, kChannelColours[tag.channel], 3.0f);
        list.AddRect(min, max, IM_COL32_BLACK, 3.0f, 0, 1.0f);
        const char digit[2] = {static_cast<char>('1' + tag.channel), '\0'};
        const float fontSize = std::min(ImGui::GetFontSize(), side);
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, digit);
        list.AddText(nullptr, fontSize,
                     {(min.x + max.x - text.x) / 2, (min.y + max.y - text.y) / 2}, IM_COL32_BLACK,
                     digit);
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
            click_ = BoardClick{at, active_, picked, component};
        }
        pressed_ = false;
        dragging_ = false;
    }
    if (!hoveredPanel) {
        return;
    }
    if (io.MouseWheel != 0) {
        viewport_.zoomAt(mouse, io.MouseWheel);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed_ = true;
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

std::optional<BoardClick> BoardView::takeClick() { return std::exchange(click_, std::nullopt); }

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
        if (hits.clicked->pad && scene_) {
            if (const auto net = scene_->padNet(hits.clicked->component, *hits.clicked->pad)) {
                net_ = net;
            }
        }
    }
}

void BoardView::setProbeTip(std::optional<Point> tip) { tip_ = tip; }

}  // namespace bw
