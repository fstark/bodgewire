#include "ui/bench.hpp"

#include <array>
#include <cfloat>
#include <optional>
#include <vector>

#include <imgui_internal.h>

#include "ui/shelf.hpp"

namespace bw {

namespace {

constexpr float kCloseSize = 14;
constexpr ImU32 kCloseFaint = IM_COL32(200, 200, 200, 70);
constexpr ImU32 kCloseBright = IM_COL32(240, 240, 240, 230);
constexpr ImU32 kCloseDisc = IM_COL32(0, 0, 0, 120);

float aspect_ = 1;  // read by the size callback during the next Begin
constexpr std::size_t kObjects = static_cast<std::size_t>(BenchObject::Debug) + 1;
std::array<bool, kObjects> popped_{};                 // in its own desktop window (P6j)
std::array<bool, kObjects> popAtStart_{};             // --pop: beside from its first place
std::array<std::optional<ImVec2>, kObjects> at_{};    // PopOut: its next position, once
std::array<std::optional<ImVec2>, kObjects> size_{};  // popped in: its next size, once
std::array<Place, kObjects> benchAt_{};  // its offset on the bench before it popped out (P6o)

std::size_t Index(BenchObject object) { return static_cast<std::size_t>(object); }

Area MainArea() {
    const ImGuiViewport* main = ImGui::GetMainViewport();
    return {main->Pos.x, main->Pos.y, main->Size.x, main->Size.y};
}

std::vector<Area> Displays() {
    std::vector<Area> displays;
    for (const ImGuiPlatformMonitor& m : ImGui::GetPlatformIO().Monitors) {
        displays.push_back({m.WorkPos.x, m.WorkPos.y, m.WorkSize.x, m.WorkSize.y});
    }
    return displays;
}

void PopOutFrom(BenchObject object, Area rect) {
    const BenchRect bench = BenchArea();
    benchAt_[Index(object)] = {rect.x - bench.min.x, rect.y - bench.min.y, 0, 0};
    popped_[Index(object)] = true;
    if (const auto beside = BesidePlace(rect, MainArea(), Displays())) {
        at_[Index(object)] = ImVec2{beside->x, beside->y};
    }
}

void PopInSized(BenchObject object, ImVec2 size) {
    const BenchRect bench = BenchArea();
    popped_[Index(object)] = false;
    const Extent fit = FitBench({size.x, size.y}, bench.extent());
    size_[Index(object)] = ImVec2{fit.width, fit.height};
    const Place at = ReturnPlace(benchAt_[Index(object)], fit, bench.extent());
    at_[Index(object)] = ImVec2{bench.min.x + at.x, bench.min.y + at.y};
}

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
    const std::size_t i = Index(object);
    if (popAtStart_[i]) {
        popAtStart_[i] = false;
        PopOutFrom(object, {bench.min.x + place.x - place.pivotX * first.x,
                            bench.min.y + place.y - place.pivotY * first.y, first.x, first.y});
    }
    if (popped_[i]) {
        // Its own viewport even over the main window (ImGui's GetWindowAlwaysWantOwnViewport).
        ImGuiWindowClass own;
        own.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&own);
    } else {
        OnBench();
    }
    if (at_[i]) {
        ImGui::SetNextWindowPos(*at_[i], ImGuiCond_Always);
        at_[i].reset();
    } else {
        ImGui::SetNextWindowPos({bench.min.x + place.x, bench.min.y + place.y},
                                ImGuiCond_FirstUseEver, {place.pivotX, place.pivotY});
    }
    if (size_[i]) {
        ImGui::SetNextWindowSize(*size_[i], ImGuiCond_Always);
        size_[i].reset();
    } else {
        ImGui::SetNextWindowSize(first, ImGuiCond_FirstUseEver);
    }
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

void TogglePop(BenchObject object, const char* window) {
    const ImGuiWindow* found = window != nullptr ? ImGui::FindWindowByName(window) : nullptr;
    if (found == nullptr) {
        if (!popped_[Index(object)]) {
            PopOutAtStart(object);  // never drawn: from its default place, when it first comes out
        }
        return;
    }
    if (popped_[Index(object)]) {
        PopInSized(object, found->Size);
    } else {
        PopOutFrom(object, {found->Pos.x, found->Pos.y, found->Size.x, found->Size.y});
    }
}

bool PoppedOut(BenchObject object) { return popped_[Index(object)]; }

void PopOutAtStart(BenchObject object) {
    popped_[Index(object)] = true;
    popAtStart_[Index(object)] = true;
}

void OnBench() { ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); }

void SharpFont(ImFont* font) {
    if (font != nullptr) {
        font->CurrentRasterizerDensity = ImGui::GetWindowViewport()->FramebufferScale.x;
    }
}

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
