#include "ui/meter_window.hpp"

#include <array>
#include <format>
#include <string>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/board_view.hpp"
#include "ui/controls.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/shelf_drawings.hpp"

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

bool MeterWindow::draw(bool& open, Session& session) {
    if (!open) {
        return false;
    }
    if (!BeginInstrument(kMeterWindowName, open, BenchObject::Meter)) {
        ImGui::End();
        return false;
    }
    const bool clicked = InstrumentClicked();
    const MeterMode mode = session.meter().mode();
    constexpr std::array<const char*, 2> kModes{"V", "Continuity"};
    std::size_t choice = mode == MeterMode::Continuity ? 1 : 0;
    if (Choice("##mode", kModes, choice)) {
        session.meter().setMode(choice == 1 ? MeterMode::Continuity : MeterMode::Volts);
    }

    if (mode == MeterMode::Continuity) {  // the Shelf's power switch, as tall as the readout
        const float side = ImGui::GetStyle().FontSizeBase * kReadoutScale;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##power", {side, side})) {
            powerClicked_ = true;
        }
        ImGui::SetItemTooltip("%s", session.powered() ? "Power: on; click to power off"
                                                      : "Power: off; click to power on");
        ItemLook look;
        look.lit = session.powered();
        look.limit = session.currentLimited();
        DrawItem(*ImGui::GetWindowDrawList(), Item::Power, at, {at.x + side, at.y + side}, look);
        ImGui::SameLine(0, 12);
    }
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kReadoutScale);
    if (mode == MeterMode::Volts) {
        if (const auto volts = session.meter().volts()) {
            ImGui::Text("%.2f V", *volts);
        } else {
            ImGui::TextColored(kGrey, "- - -");
        }
    } else if (session.powered()) {
        ImGui::TextColored(kGrey, "- - -");  // the instruction below: beside the switch, the
                                             // large words read as its label
    } else if (const auto beep = session.meter().continuity()) {
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
    if (mode == MeterMode::Continuity && session.powered()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kAmber);
        ImGui::TextWrapped("Needs the power off: click the switch");
        ImGui::PopStyleColor();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(LeadText("red", session.meter().red()).c_str());
    if (session.meter().red() || session.meter().black()) {
        ImGui::SameLine();
        if (ImGui::Button("\u00d7", {kControl, kControl})) {
            session.meter().liftLeads();
        }
        ImGui::SetItemTooltip("Lift both leads");
    }
    if (mode == MeterMode::Volts) {
        ImGui::TextColored(kGrey, "black: GND (implied)");
    } else {
        ImGui::TextUnformatted(LeadText("black", session.meter().black()).c_str());
    }
    PopOnDoubleClick(BenchObject::Meter);
    ImGui::End();
    return clicked;
}

}  // namespace bw
