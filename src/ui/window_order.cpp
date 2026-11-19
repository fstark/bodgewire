#include "ui/window_order.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

namespace bw {

bool InFront(const char* name) { return InFrontOf(name, {}); }

bool InFrontOf(const char* name, std::span<const char* const> group) {
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    constexpr ImGuiWindowFlags kNotFloating = ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup |
                                              ImGuiWindowFlags_NoBringToFrontOnFocus;
    // g.Windows is in display order, back to front.
    for (int i = g.Windows.Size - 1; i >= 0; --i) {
        const ImGuiWindow* window = g.Windows[i];
        if (!window->WasActive || window->Hidden || (window->Flags & kNotFloating) != 0) {
            continue;
        }
        if (window->ID == ImHashStr(name)) {
            return true;
        }
        if (std::ranges::none_of(
                group, [&](const char* other) { return window->ID == ImHashStr(other); })) {
            return false;
        }
    }
    return false;
}

void BringToFront(const char* name) { ImGui::SetWindowFocus(name); }

}  // namespace bw
