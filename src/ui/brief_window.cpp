#include "ui/brief_window.hpp"

#include <string>

#include <imgui.h>

#include "ui/bench.hpp"

namespace bw {

namespace {

constexpr float kTitleScale = 1.3f;
constexpr ImVec4 kPaper{0.98f, 0.92f, 0.55f, 1.0f};
constexpr ImVec4 kPaperTitle{0.93f, 0.84f, 0.40f, 1.0f};
constexpr ImVec4 kInk{0.12f, 0.11f, 0.09f, 1.0f};

}  // namespace

void DrawBrief(bool& open, const Loaded& loaded) {
    if (!open || loaded.brief.empty()) {
        return;
    }
    const Extent first = FirstSize(BenchObject::Brief, BenchArea().extent());
    PlaceNext(BenchObject::Brief, {first.width, 0});  // as tall as its text
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPaper);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, kPaperTitle);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, kPaperTitle);
    const std::string title = std::string("Puzzle brief") + kBriefWindowName;
    if (ImGui::Begin(title.c_str(), &open,
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        ImGui::PushStyleColor(ImGuiCol_Text, kInk);  // after Begin: the title stays light
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
        ImGui::TextWrapped("%s", loaded.title.c_str());
        ImGui::PopFont();
        ImGui::TextWrapped("%s", loaded.brief.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
}

}  // namespace bw
