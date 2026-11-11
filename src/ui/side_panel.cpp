#include "ui/side_panel.hpp"

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ui/toolbar.hpp"

namespace bw {

namespace {

constexpr ImU32 kLampOff = IM_COL32(70, 70, 70, 255);
constexpr ImU32 kLampHigh = IM_COL32(230, 40, 40, 255);
constexpr ImU32 kLampLow = IM_COL32(40, 200, 60, 255);
constexpr ImU32 kLampPulse = IM_COL32(255, 170, 0, 255);
constexpr float kTitleScale = 1.3f;
constexpr double kKnobKeyRate = 1.0;     // turn per second while a knob key is held
constexpr double kKnobWheelStep = 0.01;  // turn per wheel notch

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

// Up and Down turn the first knob while held, also with the section collapsed; a slider per
// controller, dragged or turned by the mouse wheel.
void DrawKnobs(Session& session) {
    const std::vector<std::string> knobs = session.knobs();
    if (knobs.empty()) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    // One knob: Up and Down. Two or more: Q and A turn the first, Up and Down the second.
    const bool players = knobs.size() >= 2;
    const auto turn = [&](const std::string& reference, ImGuiKey up, ImGuiKey down) {
        const int direction =
            static_cast<int>(ImGui::IsKeyDown(up)) - static_cast<int>(ImGui::IsKeyDown(down));
        if (direction != 0) {
            session.setKnob(reference,
                            session.knob(reference) + direction * kKnobKeyRate * io.DeltaTime);
        }
    };
    if (!io.WantTextInput) {
        if (players) {
            turn(knobs[0], ImGuiKey_Q, ImGuiKey_A);
            turn(knobs[1], ImGuiKey_UpArrow, ImGuiKey_DownArrow);
        } else {
            turn(knobs[0], ImGuiKey_UpArrow, ImGuiKey_DownArrow);
        }
    }
    if (!ImGui::CollapsingHeader("Knob", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    for (std::size_t i = 0; i < knobs.size(); ++i) {
        const std::string& reference = knobs[i];
        float percent = static_cast<float>(session.knob(reference) * 100.0);
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize(reference.c_str()).x -
                                ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::SliderFloat(reference.c_str(), &percent, 0.0f, 100.0f, "%.0f %%")) {
            session.setKnob(reference, percent / 100.0);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
            if (io.MouseWheel != 0.0f) {
                session.setKnob(reference,
                                session.knob(reference) + io.MouseWheel * kKnobWheelStep);
            }
            const std::size_t upDown = players ? 1 : 0;  // the knob Up and Down turn
            const char* keys = players && i == 0 ? "Turn the knob: drag, mouse wheel, or Q and A"
                               : i == upDown ? "Turn the knob: drag, mouse wheel, or Up and Down"
                                             : "Turn the knob: drag or mouse wheel";
            ImGui::SetTooltip("%s", keys);
        }
    }
}

// "HELLO[CR]": the keys sent, the two controls named.
std::string SentText(std::string_view codes) {
    std::string text;
    for (const char c : codes) {
        text += c == 0x0D   ? std::string("[CR]")
                : c == 0x08 ? std::string("[BS]")
                            : std::string(1, c);
    }
    return text;
}

// F2 starts typing, also with the section collapsed; a toggle and the keys sent.
void DrawKeyboard(Session& session, KeyboardCapture& capture) {
    if (capture.reference.empty()) {
        return;
    }
    if (!capture.on && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        capture.on = true;
    }
    if (!ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (ToolButton(capture.on ? "Typing (Esc)" : "Type (F2)", capture.on)) {
        capture.on = !capture.on;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Type on the Machine's keyboard (%s): every key goes to it until Esc",
                          capture.reference.c_str());
    }
    ImGui::TextUnformatted(("Sent: " + SentText(session.typed(capture.reference))).c_str());
}

}  // namespace

void SidePanel::draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
                     KeyboardCapture& capture, DebugStats stats, std::uint64_t& seed) {
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

    DrawKnobs(session);
    DrawKeyboard(session, capture);

    if (session.machine() && ImGui::CollapsingHeader("Probe", open)) {
        DrawLamps(session.lamps());
        std::string text = "not probing";
        if (const auto tip = session.probeTip(); tip && scene) {
            const auto picked = scene->pick(tip->at, tip->layer, {});
            text = picked ? scene->hoverText(picked->hit) : std::string("bare board");
            if (const auto side =
                    picked ? ContactSide(*scene, picked->hit, tip->layer) : std::string_view();
                !side.empty()) {
                text += " \u00b7 " + std::string(side);
            }
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
