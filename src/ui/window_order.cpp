#include "ui/window_order.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace bw {

bool InFront(const char* name) {
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    constexpr ImGuiWindowFlags kNotFloating = ImGuiWindowFlags_ChildWindow |
                                              ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup |
                                              ImGuiWindowFlags_NoBringToFrontOnFocus;
    // g.Windows is in display order, back to front.
    for (int i = g.Windows.Size - 1; i >= 0; --i) {
        const ImGuiWindow* window = g.Windows[i];
        if (window->WasActive && !window->Hidden && (window->Flags & kNotFloating) == 0) {
            return window->ID == ImHashStr(name);
        }
    }
    return false;
}

void BringToFront(const char* name) { ImGui::SetWindowFocus(name); }

}  // namespace bw
