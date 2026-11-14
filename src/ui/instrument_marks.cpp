#include "ui/instrument_marks.hpp"

#include <cmath>
#include <string>

#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr ImU32 kMarkText = IM_COL32(23, 24, 27, 255);

std::string OffScreenText(Time distance) { return "T " + FormatTime(std::abs(distance), false); }

float OffScreenWidth(Time distance) {
    return ImGui::CalcTextSize(OffScreenText(distance).c_str()).x + 18;
}

}  // namespace

void DrawGrid(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float top,
              float bottom, ImU32 colour) {
    for (const GridLine& line : grid) {
        const float at = std::round(x(line.time));
        list.AddLine({at, top}, {at, bottom}, colour);
    }
}

void DrawAxisLabels(ImDrawList& list, const std::vector<GridLine>& grid, const TimeToX& x, float y,
                    float left, float right, std::optional<Time> offScreenTrigger) {
    list.PushClipRect({left, y}, {right, y + kAxisHeight}, true);
    float from = left, to = right;  // where labels may go
    if (offScreenTrigger) {
        (*offScreenTrigger < 0 ? from : to) +=
            (*offScreenTrigger < 0 ? 1 : -1) * OffScreenWidth(*offScreenTrigger);
    }
    for (const GridLine& line : grid) {
        const float at = std::round(x(line.time));
        list.AddLine({at, y + kAxisHeight - 5}, {at, y + kAxisHeight}, kAxisText);
        const std::string label = GridLabel(line.time);
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

void DrawOffScreenTrigger(ImDrawList& list, Time distance, float left, float right, float y) {
    const std::string text = OffScreenText(distance);
    const float mid = y + kAxisHeight / 2;
    const float width = OffScreenWidth(distance);
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
    list.AddRectFilled({x - 7, top}, {x + 7, top + 13}, kMarkerLine, 2.0f);
    const ImVec2 size = ImGui::CalcTextSize("M");
    list.AddText({x - size.x / 2, top + (13 - size.y) / 2}, kMarkText, "M");
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

}  // namespace bw
