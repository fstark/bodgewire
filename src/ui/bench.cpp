#include "ui/bench.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <optional>
#include <vector>

#include <imgui_internal.h>

#include "ui/shelf.hpp"

namespace bw {

namespace {

constexpr float kCloseSize = 22;  // the disc (P8f: 14, and faint until hovered)
constexpr float kCloseHit = 40;   // its item: the window's corner
constexpr ImU32 kCloseFaint = IM_COL32(215, 215, 215, 150);
constexpr ImU32 kCloseBright = IM_COL32(240, 240, 240, 230);
constexpr ImU32 kCloseDisc = IM_COL32(0, 0, 0, 120);

float aspect_ = 1;                              // read by the size callback during the next Begin
std::array<bool, kBenchObjects> popped_{};      // in its own desktop window (P6j)
std::array<bool, kBenchObjects> popAtStart_{};  // --pop: beside from its first place
std::array<std::optional<ImVec2>, kBenchObjects> at_{};    // PopOut: its next position, once
std::array<std::optional<ImVec2>, kBenchObjects> size_{};  // popped in: its next size, once
std::array<Place, kBenchObjects> benchAt_{};  // its offset on the bench before it popped out (P6o)
std::array<std::optional<Area>, kBenchObjects> filledFrom_{};  // filling the bench: where it was
// A fill or its undoing waits in at_: the double-click that asked for it has begun to move the
// window (ImGui, at that frame's end), and the move would put it back where it was.
bool letGo_ = false;

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

// The objects whose window keeps its proportions (KeepAspect).
bool KeepsAspect(BenchObject object) {
    return object == BenchObject::Tv || object == BenchObject::Keyboard ||
           object == BenchObject::Knob;
}

void FillFrom(BenchObject object, Area from) {
    const BenchRect bench = BenchArea();
    const Extent room = bench.extent();
    const Extent size = FillBench(
        room, KeepsAspect(object) ? std::optional(from.height / from.width) : std::nullopt);
    filledFrom_[Index(object)] = from;
    letGo_ = true;
    size_[Index(object)] = ImVec2{size.width, size.height};
    at_[Index(object)] = ImVec2{bench.min.x + (room.width - size.width) / 2,
                                bench.min.y + (room.height - size.height) / 2};
}

void PopOutFrom(BenchObject object, Area rect) {
    filledFrom_[Index(object)].reset();
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
    ImVec2 firstAt{bench.min.x + place.x, bench.min.y + place.y};
    ImVec2 pivot{place.pivotX, place.pivotY};
    if (first.x > 0 && first.y > 0) {  // not a window that sizes itself (the brief)
        // On a bench smaller than it (P8f): shrunk in proportion, and moved in from its default
        // place as an object popped back in is.
        const Extent fit = FitBench({first.x, first.y}, bench.extent());
        first = {fit.width, fit.height};
        const Place in = ReturnPlace(
            {place.x - place.pivotX * fit.width, place.y - place.pivotY * fit.height, 0, 0}, fit,
            bench.extent());
        firstAt = {bench.min.x + in.x, bench.min.y + in.y};
        pivot = {0, 0};
    }
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
        if (letGo_ && ImGui::GetCurrentContext()->MovingWindow != nullptr) {
            ImGui::ClearActiveID();
            ImGui::StopMouseMovingWindow();
        }
        letGo_ = false;
        ImGui::SetNextWindowPos(*at_[i], ImGuiCond_Always);
        at_[i].reset();
    } else {
        ImGui::SetNextWindowPos(firstAt, ImGuiCond_FirstUseEver, pivot);
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

void PopOnDoubleClick(BenchObject object) {
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
        !ImGui::IsAnyItemHovered()) {
        TogglePop(object, ImGui::GetCurrentWindow()->Name);
    }
}

void FillBenchWith(BenchObject object) {
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    FillFrom(object, {pos.x, pos.y, size.x, size.y});
}

void Unfill(BenchObject object, std::optional<Extent> size) {
    const std::size_t i = Index(object);
    if (!filledFrom_[i]) {
        return;
    }
    const Area from = *filledFrom_[i];
    filledFrom_[i].reset();
    letGo_ = true;
    // As an object popped back in: inside the bench as it is now.
    const BenchRect bench = BenchArea();
    const Extent fit = FitBench(size ? *size : Extent{from.width, from.height}, bench.extent());
    size_[i] = ImVec2{fit.width, fit.height};
    const Place at =
        ReturnPlace({from.x - bench.min.x, from.y - bench.min.y, 0, 0}, fit, bench.extent());
    at_[i] = ImVec2{bench.min.x + at.x, bench.min.y + at.y};
}

bool Filled(BenchObject object) { return filledFrom_[Index(object)].has_value(); }

void TogglePop(BenchObject object, const char* window) {
    const ImGuiWindow* found = window != nullptr ? ImGui::FindWindowByName(window) : nullptr;
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0) {
        // No desktop windows (P8f): the bench filled, and back.
        if (Filled(object)) {
            Unfill(object);
        } else if (found != nullptr) {
            FillFrom(object, {found->Pos.x, found->Pos.y, found->Size.x, found->Size.y});
        }
        return;
    }
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

void ResetBench() {
    popped_ = {};
    popAtStart_ = {};
    at_ = {};
    size_ = {};
    benchAt_ = {};
    filledFrom_ = {};
    letGo_ = false;
}

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
    // Its item is the window's top right corner, larger than the disc: found by a finger.
    ImGui::SetCursorScreenPos({pos.x + ImGui::GetWindowWidth() - kCloseHit, pos.y});
    const bool clicked = ImGui::InvisibleButton("##case close", {kCloseHit, kCloseHit});
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
