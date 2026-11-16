#include "ui/meter_window.hpp"

#include <format>
#include <string>

#include <imgui.h>

#include "ui/board_view.hpp"
#include "ui/toolbar.hpp"

namespace bw {

namespace {

constexpr float kReadoutScale = 3.0f;
constexpr ImVec4 kAmber{1.0f, 0.7f, 0.1f, 1.0f};
constexpr ImVec4 kBeep{0.3f, 1.0f, 0.4f, 1.0f};
constexpr ImVec4 kGrey{0.5f, 0.5f, 0.5f, 1.0f};
constexpr ImU32 kLampOn = IM_COL32(60, 255, 90, 255);
constexpr ImU32 kLampOff = IM_COL32(40, 60, 40, 255);

std::string LeadText(const char* name, const std::optional<TestLead>& lead) {
    return std::format("{}: {}", name, lead ? lead->label : "-");
}

}  // namespace

void MeterWindow::draw(bool& open, Session& session, BoardView& board) {
    if (!open) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({320, 200}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + viewport->WorkSize.x * 2 / 3 - 340, viewport->WorkPos.y + 130},
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Multimeter", &open)) {
        ImGui::End();
        return;
    }
    if (ToolButton("Meter (V)", board.tool() == Tool::Meter)) {
        board.setTool(Tool::Meter);
    }
    const MeterMode mode = session.meterMode();
    ImGui::SameLine(0, 16);
    if (ImGui::RadioButton("V", mode == MeterMode::Volts)) {
        session.setMeterMode(MeterMode::Volts);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Continuity", mode == MeterMode::Continuity)) {
        session.setMeterMode(MeterMode::Continuity);
    }

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kReadoutScale);
    if (mode == MeterMode::Volts) {
        if (const auto volts = session.volts()) {
            ImGui::Text("%.2f V", *volts);
        } else {
            ImGui::TextColored(kGrey, "- - -");
        }
    } else if (session.powered()) {
        ImGui::TextColored(kAmber, "power off");
    } else if (const auto beep = session.continuity()) {
        const float size = ImGui::GetFontSize();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled({at.x + size / 2, at.y + size / 2}, size * 0.4f,
                                                    *beep ? kLampOn : kLampOff);
        ImGui::Dummy({size, size});
        ImGui::SameLine();
        if (*beep) {
            ImGui::TextColored(kBeep, "BEEP");
        } else {
            ImGui::TextUnformatted("open");
        }
    } else {
        ImGui::TextColored(kGrey, "- - -");
    }
    ImGui::PopFont();

    ImGui::TextUnformatted(LeadText("red", session.red()).c_str());
    if (mode == MeterMode::Volts) {
        ImGui::TextColored(kGrey, "black: GND (implied)");
    } else {
        ImGui::TextUnformatted(LeadText("black", session.black()).c_str());
    }
    ImGui::End();
}

}  // namespace bw
