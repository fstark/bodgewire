#include "ui/side_panel.hpp"

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace bw {

namespace {

constexpr float kTitleScale = 1.3f;
constexpr double kKnobKeyRate = 1.0;     // turn per second while a knob key is held
constexpr double kKnobWheelStep = 0.01;  // turn per wheel notch

// Up and Down turn the first knob while held, also with the section collapsed or hidden from the
// shelf (`shown` false); a slider per controller, dragged or turned by the mouse wheel.
void DrawKnobs(Session& session, bool shown) {
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
    if (!shown || !ImGui::CollapsingHeader("Knob", ImGuiTreeNodeFlags_DefaultOpen)) {
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

// "HELLO[CR]^G": the keys sent, control codes named.
std::string SentText(std::string_view codes) {
    std::string text;
    for (const char c : codes) {
        const auto code = static_cast<unsigned char>(c);
        text += code == 0x0D   ? std::string("[CR]")
                : code == 0x08 ? std::string("[BS]")
                : code == 0x1B ? std::string("[ESC]")
                : code < 0x20  ? std::string{'^', static_cast<char>(code + 0x40)}
                               : std::string(1, c);
    }
    return text;
}

}  // namespace

void SidePanel::draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
                     DebugStats stats, std::uint64_t& seed, bool tvOut, bool knobOut) {
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

    if (tvOut && session.tv() && ImGui::CollapsingHeader("TV", open)) {
        const float width = ImGui::GetContentRegionAvail().x;
        tv.draw({width, width * 0.75f});
    }

    DrawKnobs(session, knobOut);

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
        if (const auto keyboards = session.keyboards(); !keyboards.empty()) {
            const std::string sent = std::format("Keys sent ({}): {}", keyboards.front(),
                                                 SentText(session.typed(keyboards.front())));
            ImGui::TextWrapped("%s", sent.c_str());
        }
    }
}

}  // namespace bw
