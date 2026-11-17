#include "ui/debug_window.hpp"

#include <format>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/bench.hpp"

namespace bw {

namespace {

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

void DrawDebug(bool& open, const Session& session, const DebugStats& stats, std::uint64_t& seed) {
    if (!open) {
        return;
    }
    const Extent first = FirstSize(BenchObject::Debug, BenchArea().extent());
    PlaceNext(BenchObject::Debug, {first.width, first.height});
    if (ImGui::Begin("Debug", &open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed);
        ImGui::SameLine();
        ImGui::TextDisabled("(next power-on)");
        const std::string line =
            std::format("t {:.3f} s  {:.2f}×  {}  {}  ring wait {:.0f}%  ui {:.1f} ms",
                        stats.simSeconds, stats.speedRatio, stats.lock, stats.state,
                        stats.ringWaitShare * 100.0, stats.uiMilliseconds);
        ImGui::TextWrapped("%s", line.c_str());
        if (const std::vector<std::string> keyboards = session.keyboards(); !keyboards.empty()) {
            const std::string sent = std::format("Keys sent ({}): {}", keyboards.front(),
                                                 SentText(session.typed(keyboards.front())));
            ImGui::TextWrapped("%s", sent.c_str());
        }
    }
    ImGui::End();
}

}  // namespace bw
