#include "ui/manual_window.hpp"

#include <utility>

#include <imgui.h>

#include "ui/bench.hpp"

namespace bw {

void ManualWindows::draw(bool& open, const Manuals& manuals, BoardView& board,
                         const LoadedCircuit& design, std::span<const std::string> log) {
    if (!open) {
        return;
    }
    const Extent first = FirstSize(BenchObject::Manual, BenchArea().extent());
    PlaceNext(BenchObject::Manual, {first.width, first.height});
    if (ImGui::Begin(kManualWindowName, &open, ImGuiWindowFlags_NoSavedSettings) &&
        ImGui::BeginTabBar("manuals")) {
        if (ImGui::BeginTabItem("User Manual")) {
            if (manuals.user) {
                board.route(user_.draw(manuals.user->blocks, board.scene(), design));
            } else {
                ImGui::TextDisabled("no user manual");
            }
            ImGui::EndTabItem();
        }
        if (const BoardScene* scene = board.scene()) {
            const ImGuiTabItemFlags select =
                std::exchange(showChip_, false) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("Chip", nullptr, select)) {
                if (const auto chip = board.chip()) {
                    board.route(chip_.draw(*scene, *chip, manuals, design));
                } else {
                    ImGui::TextDisabled("hover or click a chip");
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Repairs")) {
                if (log.empty()) {
                    ImGui::TextDisabled("none yet");
                }
                for (const std::string& entry : log) {
                    ImGui::TextUnformatted(entry.c_str());
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

}  // namespace bw
