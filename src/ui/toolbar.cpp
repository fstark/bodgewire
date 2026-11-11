#include "ui/toolbar.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include <imgui.h>

namespace bw {

namespace {

constexpr auto kFlashTime = std::chrono::milliseconds(400);
constexpr auto kReasonTime = std::chrono::seconds(2);
constexpr float kBannerScale = 1.5f;
constexpr ImU32 kBannerBackground = IM_COL32(35, 35, 40, 255);
constexpr ImU32 kBannerFlash = IM_COL32(150, 95, 0, 255);
constexpr ImVec4 kPowerOn{0.3f, 0.9f, 0.4f, 1.0f};
constexpr ImVec4 kPowerOff{0.6f, 0.6f, 0.6f, 1.0f};
constexpr ImVec4 kAmber{1.0f, 0.7f, 0.1f, 1.0f};
constexpr ImVec4 kPlain{0.92f, 0.92f, 0.92f, 1.0f};
constexpr ImVec4 kSelected{0.2f, 0.45f, 0.8f, 1.0f};
constexpr ImU32 kLimitOn = IM_COL32(230, 40, 40, 255);
constexpr ImU32 kLimitOff = IM_COL32(70, 70, 70, 255);

}  // namespace

bool ToolButton(const char* label, bool selected) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, kSelected);
    }
    const float height = ImGui::GetFrameHeight() * 1.4f;
    const bool clicked = ImGui::Button(label, {0, height});
    if (selected) {
        ImGui::PopStyleColor();
    }
    return clicked;
}

void Toolbar::refused(std::string reason) {
    const auto now = std::chrono::steady_clock::now();
    flashUntil_ = now + kFlashTime;
    if (!reason.empty()) {
        reason_ = std::move(reason);
        reasonUntil_ = now + kReasonTime;
    }
}

void Toolbar::draw(Session& session, BoardView& board, Speed& speed, std::uint64_t seed) {
    const bool powered = session.powered();
    const bool paused = powered && session.runner().status().state == RunState::Paused;
    const bool hasBoard = board.scene() != nullptr;

    // Banner.
    const bool flashing = std::chrono::steady_clock::now() < flashUntil_;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFontSize() * kBannerScale + ImGui::GetStyle().FramePadding.y * 2;
    ImGui::GetWindowDrawList()->AddRectFilled(start, {start.x + width, start.y + height},
                                              flashing ? kBannerFlash : kBannerBackground, 4.0f);
    ImGui::SetCursorScreenPos({start.x + 8, start.y + ImGui::GetStyle().FramePadding.y});
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kBannerScale);
    ImGui::TextColored(powered ? kPowerOn : kPowerOff, "%s", powered ? "POWER ON" : "POWER OFF");
    if (paused) {
        ImGui::SameLine();
        ImGui::TextColored(kPlain, "\u00b7 paused");
    }
    if (!typing.empty()) {
        // Drawn without a Board too (a netlist-only Session); the tool keys are off meanwhile.
        ImGui::SameLine(0, 40);
        ImGui::TextColored(kPlain, "Typing on %s \u00b7 Esc to stop", typing.c_str());
    } else if (hasBoard) {
        std::string action;
        bool blocked = false;
        const auto full = [](const auto& leads) {
            return std::ranges::all_of(leads, [](const auto& l) { return l.has_value(); });
        };
        switch (board.tool()) {
        case Tool::Probe:
            action = "Probe: click a pin (F.Cu: leg, B.Cu: pad) or trace";
            break;
        case Tool::Swap:
            blocked = powered;
            action = powered ? "Swap: power off first" : "Swap: click a chip";
            break;
        case Tool::Mend:
            blocked = powered;
            action = powered ? "Mend: power off first" : "Mend: click a crack";
            break;
        case Tool::Cut:
            blocked = powered;
            action = powered ? "Cut: power off first" : "Cut: click a trace or a wire";
            break;
        case Tool::Wire:
            blocked = powered;
            action = powered       ? "Wire: power off first"
                     : wireStarted ? "Wire: click the second pin"
                                   : "Wire: click the first pin";
            break;
        case Tool::Resolder:
            blocked = powered;
            action = powered ? "Resolder: power off first" : "Resolder: click a chip pin";
            break;
        case Tool::Clip:
            blocked = full(session.clips());
            action = blocked ? "Clip: all 8 channels used" : "Clip: click a pin";
            break;
        case Tool::Hook:
            blocked = full(session.hooks());
            action = blocked ? "Hook: both channels used" : "Hook: click a pin";
            break;
        case Tool::Meter:
            if (session.meterMode() == MeterMode::Volts) {
                action = "Meter: click a point (red lead)";
            } else if (powered) {
                blocked = true;
                action = "Meter: power off for continuity";
            } else {
                action = session.blackNext() ? "Meter: click the black lead"
                                             : "Meter: click the red lead";
            }
            break;
        }
        if (std::chrono::steady_clock::now() < reasonUntil_) {
            action = reason_;
            blocked = true;
        }
        ImGui::SameLine(0, 40);
        ImGui::TextColored(blocked ? kAmber : kPlain, "%s", action.c_str());
    }
    ImGui::PopFont();
    ImGui::SetCursorScreenPos({start.x, start.y + height + ImGui::GetStyle().ItemSpacing.y});

    // Buttons.
    const float buttonHeight = ImGui::GetFrameHeight() * 1.4f;
    if (ImGui::Button(powered ? "Power off" : "Power on", {0, buttonHeight})) {
        if (powered) {
            session.powerOff();
        } else {
            session.powerOn(seed);
        }
    }
    ImGui::SameLine();
    {
        const float text = ImGui::GetTextLineHeight();
        const float radius = text * 0.4f;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            {at.x + radius, at.y + buttonHeight / 2}, radius,
            session.currentLimited() ? kLimitOn : kLimitOff);
        ImGui::Dummy({2 * radius, buttonHeight});
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(
            {ImGui::GetCursorScreenPos().x, at.y + (buttonHeight - text) / 2});
        ImGui::TextUnformatted("LIMIT");
        ImGui::SetItemTooltip("Bench supply in current limit");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!powered);
    if (ImGui::Button(paused ? "Resume" : "Pause", {0, buttonHeight})) {
        if (paused) {
            session.runner().resume();
        } else {
            session.runner().pause();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ToolButton("1\u00d7", speed == Speed::RealTime)) {
        speed = Speed::RealTime;
        session.setSpeed(speed);
    }
    ImGui::SameLine(0, 2);
    if (ToolButton("Max", speed == Speed::Max)) {
        speed = Speed::Max;
        session.setSpeed(speed);
    }
    if (hasBoard) {
        ImGui::SameLine(0, 24);
        if (ToolButton("Probe (P)", board.tool() == Tool::Probe)) {
            board.setTool(Tool::Probe);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Swap (S)", board.tool() == Tool::Swap)) {
            board.setTool(Tool::Swap);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Mend (M)", board.tool() == Tool::Mend)) {
            board.setTool(Tool::Mend);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Cut (K)", board.tool() == Tool::Cut)) {
            board.setTool(Tool::Cut);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Wire (W)", board.tool() == Tool::Wire)) {
            board.setTool(Tool::Wire);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Resolder (R)", board.tool() == Tool::Resolder)) {
            board.setTool(Tool::Resolder);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Clip (C)", board.tool() == Tool::Clip)) {
            board.setTool(Tool::Clip);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Hook (O)", board.tool() == Tool::Hook)) {
            board.setTool(Tool::Hook);
        }
        ImGui::SameLine(0, 2);
        if (ToolButton("Meter (V)", board.tool() == Tool::Meter)) {
            board.setTool(Tool::Meter);
        }
        ImGui::SameLine(0, 24);
        const bool front = board.activeLayer() == Layer::Front;
        if (ToolButton(front ? "F.Cu (L)" : "B.Cu (L)", false)) {
            board.toggleLayer();
        }
        ImGui::SameLine();
        if (ToolButton("Fit (F)", false)) {
            board.fit();
        }
    }
    ImGui::SameLine(0, 24);
    if (ToolButton("User Manual (U)", userManualOpen)) {
        userManualOpen = !userManualOpen;
    }
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_U, false)) {
        userManualOpen = !userManualOpen;
    }
}

}  // namespace bw
