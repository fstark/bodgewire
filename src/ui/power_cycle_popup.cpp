#include "ui/power_cycle_popup.hpp"

namespace bw {

namespace {

constexpr const char* kPopup = "##power cycle";
constexpr float kTextWidth = 300;

}  // namespace

void PowerCyclePopup::open(ImVec2 at) {
    opening_ = at;
    ticked_ = false;
}

PopupAnswer PowerCyclePopup::draw() {
    if (opening_) {
        ImGui::SetNextWindowPos(*opening_);
        ImGui::OpenPopup(kPopup);
        opening_.reset();
        shown_ = true;
    }
    if (!ImGui::BeginPopup(kPopup, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_AlwaysAutoResize)) {
        if (shown_) {  // closed by a click elsewhere
            shown_ = false;
            return PopupAnswer::Cancel;
        }
        return PopupAnswer::None;
    }
    PopupAnswer answer = PopupAnswer::None;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTextWidth);
    ImGui::TextUnformatted("This tool needs the power off. OK powers the Machine off, uses the "
                           "tool and powers it on again; the Machine restarts.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(70, 74, 82, 255));  // the box seen on dark
    ImGui::Checkbox("Don't ask again", &ticked_);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    if (ImGui::Button("OK", {80, 0})) {
        dontAsk_ = dontAsk_ || ticked_;
        answer = PopupAnswer::Ok;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {80, 0})) {
        answer = PopupAnswer::Cancel;
    }
    if (answer != PopupAnswer::None) {
        ImGui::CloseCurrentPopup();
        shown_ = false;
    }
    ImGui::EndPopup();
    return answer;
}

}  // namespace bw
