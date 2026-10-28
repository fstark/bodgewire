#include "ui/board_view.hpp"

namespace bw {

namespace {

constexpr ImU32 kPanelBackground = IM_COL32(20, 20, 24, 255);

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
    }
    ImGui::EndChild();
}

void BoardView::controls() {
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
