#include "ui/side_panel.hpp"

#include <format>
#include <string>

#include <imgui.h>

namespace bw {

namespace {

constexpr ImU32 kLampOff = IM_COL32(70, 70, 70, 255);
constexpr ImU32 kLampHigh = IM_COL32(230, 40, 40, 255);
constexpr ImU32 kLampLow = IM_COL32(40, 200, 60, 255);
constexpr ImU32 kLampPulse = IM_COL32(255, 170, 0, 255);
constexpr float kTitleScale = 1.3f;

// "● HIGH  ● LOW  ● PULSE", one row.
void DrawLamps(ProbeLamps lamps) {
    const auto lamp = [](bool lit, ImU32 colour, const char* name, bool first) {
        if (!first) {
            ImGui::SameLine();
        }
        const float height = ImGui::GetTextLineHeight();
        const float radius = height * 0.4f;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled({at.x + radius, at.y + height / 2}, radius,
                                                    lit ? colour : kLampOff);
        ImGui::Dummy({2 * radius, height});
        ImGui::SameLine();
        ImGui::TextUnformatted(name);
    };
    lamp(lamps.high, kLampHigh, "HIGH", true);
    lamp(lamps.low, kLampLow, "LOW", false);
    lamp(lamps.pulse, kLampPulse, "PULSE", false);
}

void DrawNoVideo(ImVec2 size) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(at, {at.x + size.x, at.y + size.y}, IM_COL32_BLACK);
    const char* text = "no video";
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    list.AddText({at.x + (size.x - textSize.x) / 2, at.y + (size.y - textSize.y) / 2},
                 IM_COL32_WHITE, text);
    ImGui::Dummy(size);
}

}  // namespace

void SidePanel::draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
                     DebugStats stats, std::uint64_t& seed) {
    const BoardScene* scene = board.scene();
    const LoadedCircuit& design = DesignOf(session);
    const ImGuiTreeNodeFlags open = ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Puzzle", open)) {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
        ImGui::TextWrapped("%s", loaded.title.c_str());
        ImGui::PopFont();
        if (!loaded.brief.empty()) {
            ImGui::TextWrapped("%s", loaded.brief.c_str());
        }
        if (const auto& user = loaded.manuals.manuals.user;
            user && ImGui::TreeNode("When it works")) {
            board.route(puzzle_.draw(Section(*user, "What it does"), scene, design));
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("TV", open)) {
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 size{width, width * 0.75f};
        if (session.tv()) {
            tv.draw(size);
        } else {
            DrawNoVideo(size);
        }
    }

    if (session.machine() && ImGui::CollapsingHeader("Probe", open)) {
        DrawLamps(session.lamps());
        std::string text = "not probing";
        if (const auto tip = session.probeTip(); tip && scene) {
            const auto picked = scene->pick(tip->at, tip->layer, {});
            text = picked ? scene->hoverText(picked->hit) : std::string("bare board");
        }
        ImGui::TextUnformatted(text.c_str());
    }

    if (scene && ImGui::CollapsingHeader("Chip", open)) {
        if (const auto chip = board.chip()) {
            board.route(chip_.draw(*scene, *chip, loaded.manuals.manuals, design));
        } else {
            ImGui::TextDisabled("hover or click a chip");
        }
    }

    if (session.machine() && ImGui::CollapsingHeader("Repairs", open)) {
        if (session.log().empty()) {
            ImGui::TextDisabled("none yet");
        }
        for (const std::string& entry : session.log()) {
            ImGui::TextUnformatted(entry.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed);
        ImGui::SameLine();
        ImGui::TextDisabled("(next power-on)");
        const std::string line =
            std::format("t {:.3f} s  {:.2f}\u00d7  {}  {}  ring wait {:.0f}%  ui {:.1f} ms",
                        stats.simSeconds, stats.speedRatio, stats.lock, stats.state,
                        stats.ringWaitShare * 100.0, stats.uiMilliseconds);
        ImGui::TextWrapped("%s", line.c_str());
    }
}

}  // namespace bw
