#include "ui/question_popup.hpp"

#include <utility>

namespace bw {

namespace {

constexpr float kTextWidth = 300;

}  // namespace

QuestionPopup::QuestionPopup(const char* id, std::string text, bool dontAskBox)
    : id_(id), text_(std::move(text)), dontAskBox_(dontAskBox) {}

void QuestionPopup::open(ImVec2 at, ImVec2 pivot) {
    opening_ = at;
    pivot_ = pivot;
    ticked_ = false;
}

PopupAnswer QuestionPopup::draw() {
    if (opening_) {
        ImGui::SetNextWindowPos(*opening_, ImGuiCond_Always, pivot_);
        ImGui::OpenPopup(id_);
        opening_.reset();
        shown_ = true;
    }
    if (!ImGui::BeginPopup(id_, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_AlwaysAutoResize)) {
        if (shown_) {  // closed by a click elsewhere
            shown_ = false;
            return PopupAnswer::Cancel;
        }
        return PopupAnswer::None;
    }
    PopupAnswer answer = PopupAnswer::None;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTextWidth);
    ImGui::TextUnformatted(text_.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (dontAskBox_) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(70, 74, 82, 255));  // seen on dark
        ImGui::Checkbox("Don't ask again", &ticked_);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
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
