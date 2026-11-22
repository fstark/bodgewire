#include "ui/launcher.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "ui/manual_view.hpp"

namespace bw {

namespace {

constexpr float kListWidth = 220;
constexpr float kNameScale = 1.5f;
constexpr float kBriefIndent = 12;
constexpr ImVec4 kError{1.0f, 0.35f, 0.3f, 1.0f};

// A clickable entry as tall as its title and wrapped brief; true when clicked.
bool Entry(int id, const std::string& title, const std::string& brief) {
    const float width = ImGui::GetContentRegionAvail().x;
    const float wrap = width - kBriefIndent;
    const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
    const float briefHeight =
        brief.empty() ? 0 : ImGui::CalcTextSize(brief.c_str(), nullptr, false, wrap).y;
    const float gap = brief.empty() ? 0 : ImGui::GetStyle().ItemInnerSpacing.y;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    const bool clicked = ImGui::Selectable("##entry", false, ImGuiSelectableFlags_None,
                                           {0, titleSize.y + gap + briefHeight});
    ImGui::PopID();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddText(at, ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
    if (!brief.empty()) {
        list.AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                     {at.x + kBriefIndent, at.y + titleSize.y + gap},
                     ImGui::GetColorU32(ImGuiCol_TextDisabled), brief.c_str(), nullptr, wrap);
    }
    ImGui::Spacing();
    return clicked;
}

}  // namespace

void Launcher::setList(MachineList list) {
    list_ = std::move(list);
    select(selected_);
}

void Launcher::select(std::size_t machine) {
    selected_ = list_.machines.empty() ? 0 : std::min(machine, list_.machines.size() - 1);
    scroll_ = true;
}

void Launcher::setError(std::string text) { error_ = std::move(text); }

std::optional<std::filesystem::path> Launcher::draw() {
    std::optional<std::filesystem::path> chosen;
    ImGui::BeginChild("machines", {kListWidth, 0}, ImGuiChildFlags_Borders);
    for (std::size_t i = 0; i < list_.machines.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(Displayable(list_.machines[i].name).c_str(), i == selected_)) {
            selected_ = i;
            error_.clear();
        }
        if (i == selected_ && scroll_) {
            ImGui::SetScrollHereY();
            scroll_ = false;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("entries", {0, 0}, ImGuiChildFlags_Borders);
    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kError);
        ImGui::TextWrapped("%s", Displayable(error_).c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    if (list_.machines.empty()) {
        ImGui::TextUnformatted("No Machines.");
    } else {
        const MachineEntry& machine = list_.machines[selected_];
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kNameScale);
        ImGui::TextUnformatted(Displayable(machine.name).c_str());
        ImGui::PopFont();
        if (!machine.line.empty()) {
            ImGui::TextWrapped("%s", Displayable(machine.line).c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (Entry(-1, "The working Machine (no fault)", "")) {
            chosen = machine.netlist;
        }
        for (std::size_t p = 0; p < machine.puzzles.size(); ++p) {
            const PuzzleEntry& puzzle = machine.puzzles[p];
            if (Entry(static_cast<int>(p), Displayable(puzzle.title), Displayable(puzzle.brief))) {
                chosen = puzzle.file;
            }
        }
    }
    ImGui::EndChild();
    return chosen;
}

}  // namespace bw
