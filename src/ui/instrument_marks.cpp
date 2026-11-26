#include "ui/instrument_marks.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui_internal.h>

#include "ui/bench.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr ImU32 kMarkText = IM_COL32(23, 24, 27, 255);

// To a tenth of a division, as the scope's position is set.
std::string OffScreenText(Time distance, Time perDivision) {
    return "T " + FormatTimeStep(std::abs(distance), perDivision / 10, false);
}

float OffScreenWidth(Time distance, Time perDivision) {
    return ImGui::CalcTextSize(OffScreenText(distance, perDivision).c_str()).x + 18;
}

}  // namespace

std::vector<GridLine> LabelledGrid(const TimeView& view, float width) {
    // The widest label is at an end of the view: the most digits before the point.
    float widest = 0;
    for (const Time t : {view.left, view.left + view.span()}) {
        const Time line = t / view.perDivision * view.perDivision;
        widest = std::max(widest, ImGui::CalcTextSize(GridLabel(line, view.perDivision).c_str()).x);
    }
    return Grid(view, width, std::max(60.0f, widest + 16));
}

void DrawGrid(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float top,
              float bottom, ImU32 colour) {
    for (const GridLine& line : grid) {
        const float at = std::round(x(line.time));
        list.AddLine({at, top}, {at, bottom}, colour);
    }
}

void DrawAxisLabels(ImDrawList& list, const TimeView& view, const std::vector<GridLine>& grid,
                    const TimeToX& x, float y, float left, float right, bool arrow) {
    const std::optional<Time> offScreenTrigger = arrow ? OffScreenTrigger(view) : std::nullopt;
    list.PushClipRect({left, y}, {right, y + kAxisHeight}, true);
    float from = left, to = right;  // where labels may go
    if (offScreenTrigger) {
        (*offScreenTrigger < 0 ? from : to) +=
            (*offScreenTrigger < 0 ? 1 : -1) * OffScreenWidth(*offScreenTrigger, view.perDivision);
    }
    for (const GridLine& line : grid) {
        const float at = std::round(x(line.time));
        list.AddLine({at, y + kAxisHeight - 5}, {at, y + kAxisHeight}, kAxisText);
        const std::string label = GridLabel(line.time, view.perDivision);
        if (line.labelled && at + 3 >= from &&
            at + 3 + ImGui::CalcTextSize(label.c_str()).x <= to) {
            const ImU32 colour = line.time == 0 ? kTriggerMark : kAxisText;
            list.AddText({at + 3, y + 1}, colour, label.c_str());
        }
    }
    list.PopClipRect();
}

void DrawTrigger(ImDrawList& list, float x, float top, float bottom) {
    list.AddLine({x, top}, {x, bottom}, kTriggerMark, 1.5f);
    list.AddTriangleFilled({x - 6, top}, {x + 6, top}, {x, top + 8}, kTriggerMark);
}

void DrawOffScreenTrigger(ImDrawList& list, const TimeView& view, float left, float right,
                          float y) {
    const std::optional<Time> off = OffScreenTrigger(view);
    if (!off) {
        return;
    }
    const Time distance = *off;
    const std::string text = OffScreenText(distance, view.perDivision);
    const float mid = y + kAxisHeight / 2;
    const float width = OffScreenWidth(distance, view.perDivision);
    if (distance < 0) {
        list.AddTriangleFilled({left + 2, mid}, {left + 10, mid - 5}, {left + 10, mid + 5},
                               kTriggerMark);
        list.AddText({left + 14, y + 1}, kTriggerMark, text.c_str());
    } else {
        list.AddTriangleFilled({right - 2, mid}, {right - 10, mid - 5}, {right - 10, mid + 5},
                               kTriggerMark);
        list.AddText({right - width + 2, y + 1}, kTriggerMark, text.c_str());
    }
}

void DrawMarker(ImDrawList& list, float x, float top, float bottom) {
    list.AddLine({x, top}, {x, bottom}, kMarkerLine, 1.0f);
    list.AddRectFilled({x - kMarkerFlagWidth / 2, top},
                       {x + kMarkerFlagWidth / 2, top + kMarkerFlagHeight}, kMarkerLine, 3.0f);
    const ImVec2 size = ImGui::CalcTextSize("M");
    list.AddText({x - size.x / 2, top + (kMarkerFlagHeight - size.y) / 2}, kMarkText, "M");
}

void StatusLine(std::optional<Time> cursor, std::optional<Time> marker, const char* hint) {
    const std::string text = StatusText(cursor, marker);
    if (text.empty()) {
        ImGui::TextColored(kHintGrey, "%s", hint);
    } else {
        ImGui::TextUnformatted(text.c_str());
    }
}

int WheelNotches(float wheel, float& rest) {
    rest += wheel;
    const auto notches = static_cast<int>(std::trunc(rest));
    rest -= static_cast<float>(notches);
    return notches;
}

bool BeginInstrument(const char* name, bool& open, BenchObject object) {
    const Extent normal = NormalSize(object);
    PlaceNext(object, {normal.width, normal.height});
    if (!ImGui::Begin(name, &open,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        return false;
    }
    // The size button, in the title bar left of the close button (P6d).
    const ImRect title = ImGui::GetCurrentWindow()->TitleBarRect();
    const float side = title.GetHeight();
    const ImVec2 min{title.Max.x - 2 * side, title.Min.y};
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::PushClipRect(title.Min, title.Max, false);
    ImGui::SetCursorScreenPos(min);
    if (ImGui::InvisibleButton("##size", {side, side})) {
        const ImVec2 now = ImGui::GetWindowSize();
        const Extent minimal = MinimalSize(object);
        if (PoppedOut(object)) {
            const Extent next = NextPreset({now.x, now.y}, minimal, normal);
            ImGui::SetWindowSize({next.width, next.height});
        } else {
            // On the bench (P8f): minimal, normal, then the whole bench, and back where it was.
            const Extent full = BenchArea().extent();
            const Extent next = NextPreset({now.x, now.y}, minimal, normal, full);
            if (next.width == full.width && next.height == full.height) {
                FillBenchWith(object);
            } else if (Filled(object)) {
                Unfill(object, next);
            } else {
                ImGui::SetWindowSize({next.width, next.height});
            }
        }
    }
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImU32 colour = ImGui::GetColorU32(ImGuiCol_Text, hovered ? 1.0f : 0.6f);
    const float u = side / 16;
    list.AddRect({min.x + 4 * u, min.y + 4 * u}, {min.x + 12 * u, min.y + 12 * u}, colour, 0, 0,
                 1.0f);
    list.AddRectFilled({min.x + 4 * u, min.y + 8 * u}, {min.x + 8 * u, min.y + 12 * u}, colour);
    if (hovered) {
        ImGui::SetTooltip(PoppedOut(object) ? "Minimal / normal size"
                                            : "Minimal / normal / full size");
    }
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(cursor);
    return true;
}

bool InstrumentClicked() {
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
                                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
        !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return false;
    }
    // Not on the close button: ImGui closes at the release, a frame later, and the closing
    // window would first take the focus from the tool that has it, then give it to the probe.
    const ImVec2 pos = ImGui::GetWindowPos();
    const float right = pos.x + ImGui::GetWindowWidth();
    const float title = ImGui::GetFrameHeight();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    return !(mouse.y < pos.y + title && mouse.x > right - title);
}

}  // namespace bw
