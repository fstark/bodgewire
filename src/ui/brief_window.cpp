#include "ui/brief_window.hpp"

#include <cfloat>
#include <string>

#include "ui/bench.hpp"

namespace bw {

namespace {

constexpr float kWidth = 230;
constexpr float kPad = 12;
constexpr float kCurl = 14;  // the lifted bottom-right corner
constexpr float kTitleScale = 1.2f;
constexpr ImU32 kPaper = IM_COL32(253, 236, 120, 255);
constexpr ImU32 kGlue = IM_COL32(240, 218, 95, 255);
constexpr ImU32 kCurlUnder = IM_COL32(205, 180, 60, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 70);
constexpr ImU32 kLiftShadow = IM_COL32(0, 0, 0, 90);

}  // namespace

void DrawNote(ImDrawList& list, ImVec2 at, ImVec2 end, float lift) {
    if (lift > 0) {  // the whole note lifted off the wood: its shadow below and right
        list.PathLineTo({at.x + lift, at.y + lift});
        list.PathLineTo({end.x + lift, at.y + lift});
        list.PathLineTo({end.x + lift, end.y + lift - kCurl});
        list.PathLineTo({end.x + lift - kCurl, end.y + lift});
        list.PathLineTo({at.x + lift, end.y + lift});
        list.PathFillConvex(kLiftShadow);
    }
    // A shadow under the lifted corner, the note with its corner cut, the corner curled up.
    list.PushClipRectFullScreen();
    list.AddTriangleFilled({end.x - kCurl, end.y + 3}, {end.x + 3, end.y - kCurl},
                           {end.x + 2, end.y + 2}, kShadow);
    list.PopClipRect();
    list.PathLineTo(at);
    list.PathLineTo({end.x, at.y});
    list.PathLineTo({end.x, end.y - kCurl});
    list.PathLineTo({end.x - kCurl, end.y});
    list.PathLineTo({at.x, end.y});
    list.PathFillConvex(kPaper);
    list.AddRectFilled(at, {end.x, at.y + kNoteStrip}, kGlue);
    list.AddTriangleFilled({end.x, end.y - kCurl}, {end.x - kCurl, end.y},
                           {end.x - kCurl, end.y - kCurl}, kCurlUnder);
}

void DrawBrief(bool& open, const Loaded& loaded, std::optional<ImVec2> corner, BriefNote& note) {
    if (!open || loaded.brief.empty()) {
        return;
    }
    if (corner) {
        OnBench();
        ImGui::SetNextWindowPos({corner->x + note.offset.x, corner->y + note.offset.y},
                                ImGuiCond_Always);
    } else {
        PlaceNext(BenchObject::Brief, {kWidth, 0});
    }
    ImGui::SetNextWindowSizeConstraints({kWidth, 0}, {kWidth, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPad, kPad});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));  // the note is drawn
    const bool visible =
        ImGui::Begin(kBriefWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!visible) {
        ImGui::End();
        return;
    }
    // Dragged by the note, not its × (P6o): an open popup keeps it from being hovered.
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsAnyItemHovered()) {
        note.dragging = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        note.dragging = false;
    }
    if (note.dragging) {
        note.offset.x += ImGui::GetIO().MouseDelta.x;
        note.offset.y += ImGui::GetIO().MouseDelta.y;
    }
    const ImVec2 at = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 end{at.x + size.x, at.y + size.y};
    DrawNote(*ImGui::GetWindowDrawList(), at, end);

    ImGui::Dummy({0, kNoteStrip - 4});  // under the glue
    ImGui::PushStyleColor(ImGuiCol_Text, kNoteInk);
    ImGui::PushTextWrapPos(kWidth - kPad);
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
    ImGui::TextUnformatted(loaded.title.c_str());
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::TextUnformatted(loaded.brief.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (CaseClose(1)) {
        open = false;
    }
    ImGui::End();
}

}  // namespace bw
