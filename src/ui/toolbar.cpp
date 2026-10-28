#include "ui/toolbar.hpp"

#include <string>

#include <imgui.h>

namespace bw {

namespace {

constexpr auto kFlashTime = std::chrono::milliseconds(400);
constexpr float kBannerScale = 1.5f;
constexpr ImU32 kBannerBackground = IM_COL32(35, 35, 40, 255);
constexpr ImU32 kBannerFlash = IM_COL32(150, 95, 0, 255);
constexpr ImVec4 kPowerOn{0.3f, 0.9f, 0.4f, 1.0f};
constexpr ImVec4 kPowerOff{0.6f, 0.6f, 0.6f, 1.0f};
constexpr ImVec4 kAmber{1.0f, 0.7f, 0.1f, 1.0f};
constexpr ImVec4 kPlain{0.92f, 0.92f, 0.92f, 1.0f};
constexpr ImVec4 kSelected{0.2f, 0.45f, 0.8f, 1.0f};

// A large toggle button; true when clicked.
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

}  // namespace

void Toolbar::refused() { flashUntil_ = std::chrono::steady_clock::now() + kFlashTime; }

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
    if (hasBoard) {
        const char* action = "";
        bool blocked = false;
        switch (board.tool()) {
        case Tool::Probe:
            action = "Probe: click a pin or trace";
            break;
        case Tool::Swap:
            blocked = powered;
            action = powered ? "Swap: power off first" : "Swap: click a chip";
            break;
        case Tool::Mend:
            blocked = powered;
            action = powered ? "Mend: power off first" : "Mend: click a crack";
            break;
        }
        ImGui::SameLine(0, 40);
        ImGui::TextColored(blocked ? kAmber : kPlain, "%s", action);
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
}

}  // namespace bw
