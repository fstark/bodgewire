#include "ui/tv_window.hpp"

#include <imgui.h>

#include "ui/bench.hpp"
#include "view/bench_places.hpp"

namespace bw {

namespace {

constexpr float kMinWidth = 200;
constexpr ImU32 kCase = IM_COL32(52, 52, 56, 255);
constexpr ImU32 kCaseLight = IM_COL32(84, 84, 90, 255);
constexpr ImU32 kCaseDark = IM_COL32(30, 30, 33, 255);
constexpr ImU32 kWell = IM_COL32(6, 6, 7, 255);
constexpr ImU32 kKnob = IM_COL32(24, 24, 26, 255);
constexpr ImU32 kKnobRim = IM_COL32(96, 96, 100, 255);
constexpr ImU32 kLampOn = IM_COL32(70, 230, 90, 255);
constexpr ImU32 kLampOff = IM_COL32(30, 60, 36, 255);

}  // namespace

void TvWindow::draw(bool& open, const TvView& view, bool powered) {
    if (!open) {
        return;
    }
    PlaceNext(BenchObject::Tv, {kTvFirstWidth, kTvFirstWidth * kTvAspect});
    KeepAspect(kTvAspect, kMinWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kCase);
    const bool visible =
        ImGui::Begin(kTvWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!visible) {
        ImGui::End();
        return;
    }
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 at = ImGui::GetWindowPos();
    const float w = ImGui::GetWindowWidth();
    const float h = ImGui::GetWindowHeight();
    // The case: a lighter top edge, a darker chin line.
    list.AddRect(at, {at.x + w, at.y + h}, kCaseLight, 8.0f, 0, 1.5f);
    // The picture's well and the picture.
    const float bezel = kTvBezel * w;
    const ImVec2 picture{at.x + bezel, at.y + bezel};
    const ImVec2 size{kTvPicture * w, kTvPicture * w * 0.75f};
    const float lip = 0.012f * w;
    list.AddRectFilled({picture.x - lip, picture.y - lip},
                       {picture.x + size.x + lip, picture.y + size.y + lip}, kWell, lip * 2);
    list.AddRect({picture.x - lip, picture.y - lip},
                 {picture.x + size.x + lip, picture.y + size.y + lip}, kCaseDark, lip * 2, 0, lip);
    ImGui::SetCursorScreenPos(picture);
    ImGui::Image(view.texture(), size);
    // The chin: a pilot lamp, two knobs on the right (position controls come in P6h).
    const float chinTop = picture.y + size.y + lip;
    const float chinMid = (chinTop + at.y + h) / 2;
    const float unit = 0.01f * w;
    list.AddCircleFilled({at.x + bezel + 3 * unit, chinMid}, 1.2f * unit,
                         powered ? kLampOn : kLampOff);
    for (int i = 0; i < 2; ++i) {
        const ImVec2 c{at.x + w - bezel - (3 + 7 * static_cast<float>(i)) * unit, chinMid};
        list.AddCircleFilled(c, 2.4f * unit, kKnob);
        list.AddCircle(c, 2.4f * unit, kKnobRim, 0, 1.0f);
        list.AddLine(c, {c.x, c.y - 2.0f * unit}, kKnobRim, 1.0f);
    }
    if (CaseClose()) {
        open = false;
    }
    ImGui::End();
}

}  // namespace bw
