#include "ui/bench.hpp"

#include <array>
#include <cfloat>

#include "ui/shelf.hpp"

namespace bw {

namespace {

constexpr float kCloseSize = 14;
constexpr ImU32 kCloseFaint = IM_COL32(200, 200, 200, 70);
constexpr ImU32 kCloseBright = IM_COL32(240, 240, 240, 230);
constexpr ImU32 kCloseDisc = IM_COL32(0, 0, 0, 120);

float aspect_ = 1;  // read by the size callback during the next Begin
std::array<bool, static_cast<std::size_t>(BenchObject::Debug) + 1> home_{};  // SendHome

}  // namespace

BenchRect BenchArea() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 min{viewport->WorkPos.x + style.WindowPadding.x,
                     viewport->WorkPos.y + style.WindowPadding.y};
    const ImVec2 max{viewport->WorkPos.x + viewport->WorkSize.x - style.WindowPadding.x,
                     viewport->WorkPos.y + viewport->WorkSize.y - style.WindowPadding.y -
                         kShelfHeight - style.ItemSpacing.y};
    return {min, max};
}

void PlaceNext(BenchObject object, ImVec2 first) {
    const BenchRect bench = BenchArea();
    const Place place = DefaultPlace(object, bench.extent());
    bool& home = home_[static_cast<std::size_t>(object)];
    ImGui::SetNextWindowPos({bench.min.x + place.x, bench.min.y + place.y},
                            home ? ImGuiCond_Always : ImGuiCond_FirstUseEver,
                            {place.pivotX, place.pivotY});
    home = false;
    ImGui::SetNextWindowSize(first, ImGuiCond_FirstUseEver);
}

void KeepAspect(float aspect, float minWidth) {
    aspect_ = aspect;
    ImGui::SetNextWindowSizeConstraints(
        {minWidth, minWidth * aspect}, {FLT_MAX, FLT_MAX},
        [](ImGuiSizeCallbackData* data) {
            data->DesiredSize.y = data->DesiredSize.x * *static_cast<const float*>(data->UserData);
        },
        &aspect_);
}

void SendHome(BenchObject object) { home_[static_cast<std::size_t>(object)] = true; }

bool CaseClose(float inset) {
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 min{pos.x + ImGui::GetWindowWidth() - inset - kCloseSize, pos.y + inset};
    ImGui::SetCursorScreenPos(min);
    const bool clicked = ImGui::InvisibleButton("##case close", {kCloseSize, kCloseSize});
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 c{min.x + kCloseSize / 2, min.y + kCloseSize / 2};
    const float r = kCloseSize / 2;
    list.AddCircleFilled(c, r, kCloseDisc);
    const ImU32 colour = hovered ? kCloseBright : kCloseFaint;
    list.AddCircle(c, r, colour, 0, 1.0f);
    const float d = r * 0.45f;
    list.AddLine({c.x - d, c.y - d}, {c.x + d, c.y + d}, colour, 1.5f);
    list.AddLine({c.x - d, c.y + d}, {c.x + d, c.y - d}, colour, 1.5f);
    if (hovered) {
        ImGui::SetTooltip("Put away");
    }
    return clicked;
}

}  // namespace bw
