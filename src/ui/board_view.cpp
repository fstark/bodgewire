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

void DrawCourtyard(ImDrawList& list, const Mapper& map, const BoardComponent& component,
                   std::span<const Point> courtyard) {
    std::vector<ImVec2> points;
    ImVec2 centre{0, 0};
    for (const Point p : courtyard) {
        points.push_back(map(p));
        centre = {centre.x + points.back().x, centre.y + points.back().y};
    }
    const auto n = static_cast<float>(points.size());
    centre = {centre.x / n, centre.y / n};
    list.AddPolyline(points.data(), static_cast<int>(points.size()), kCourtyard, ImDrawFlags_Closed,
                     1.0f);

    const float size = std::clamp(static_cast<float>(kReferenceMm * map.pixelsPerMm()),
                                  kReferenceMinPx, kReferenceMaxPx);
    ImFont* font = ImGui::GetFont();
    const char* text = component.reference.c_str();
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    list.AddText(font, size, {centre.x - textSize.x / 2, centre.y - textSize.y / 2}, kReference,
                 text);

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
    input(origin);

    const Mapper map(viewport_, origin, panel);
    const Board& board = scene_->board();
    std::vector<ImVec2> outline;
    for (const Point p : scene_->outline()) {
        outline.push_back(map(p));
    }
    list.AddConcavePolyFilled(outline.data(), static_cast<int>(outline.size()), kBoardGreen);

    const std::optional<NetId> lit = highlight_;
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
        DrawCourtyard(list, map, board.components[k], scene_->courtyard(k));
    }
    if (tip_) {
        const float radius = std::max(4.0f, map.pixels(400'000));
        list.AddCircleFilled(map(*tip_), radius, kProbeRed);
        list.AddCircle(map(*tip_), radius, IM_COL32_WHITE, 0, 1.0f);
    }
    ImGui::EndChild();
}

void BoardView::input(ImVec2 origin) {
    highlight_.reset();
    if (!ImGui::IsWindowHovered()) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const ScreenPoint mouse{io.MousePos.x - origin.x, io.MousePos.y - origin.y};
    if (io.MouseWheel != 0) {
        viewport_.zoomAt(mouse, io.MouseWheel);
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0)) {
        viewport_.pan({io.MouseDelta.x, io.MouseDelta.y});
    }
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_L, false)) {
            active_ = active_ == Layer::Front ? Layer::Back : Layer::Front;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            viewport_.fit(scene_->bounds(), {panel_.x, panel_.y});
        }
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
            tool_ = Tool::Probe;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            tool_ = Tool::Swap;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_M, false)) {
            tool_ = Tool::Mend;
        }
    }
    const Point at = viewport_.toBoard(mouse);
    const auto picked = scene_->pick(at, active_, {});
    const auto hover = picked ? std::optional(picked->hit) : std::nullopt;
    if (hover) {
        highlight_ = scene_->netOf(*hover);
        const std::string text = scene_->hoverText(*hover);
        if (!text.empty()) {
            ImGui::SetTooltip("%s", text.c_str());
        }
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        click_ = BoardClick{at, picked ? picked->layer : active_,
                            hover && hover->kind != HitKind::Component, scene_->componentAt(at)};
    }
}

std::optional<BoardClick> BoardView::takeClick() { return std::exchange(click_, std::nullopt); }

void BoardView::setProbeTip(std::optional<Point> tip) { tip_ = tip; }

void BoardView::controls(bool powered) {
    ImGui::TextUnformatted("Tool ");
    ImGui::SameLine();
    const auto toolButton = [&](const char* label, Tool tool) {
        if (ImGui::RadioButton(label, tool_ == tool)) {
            tool_ = tool;
        }
    };
    toolButton("Probe", Tool::Probe);
    ImGui::SameLine();
    toolButton("Swap", Tool::Swap);
    ImGui::SameLine();
    toolButton("Mend", Tool::Mend);
    if (powered) {
        ImGui::SameLine();
        ImGui::TextDisabled("power off to repair");
    }
    ImGui::TextUnformatted("Layer");
    ImGui::SameLine();
    if (ImGui::RadioButton("F.Cu", active_ == Layer::Front)) {
        active_ = Layer::Front;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("B.Cu", active_ == Layer::Back)) {
        active_ = Layer::Back;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        needFit_ = true;
    }
}

}  // namespace bw
