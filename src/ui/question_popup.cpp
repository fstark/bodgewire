#include "ui/question_popup.hpp"

#include <algorithm>
#include <utility>

#include "ui/controls.hpp"

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

void QuestionPopup::openAbove(ImVec2 pointer) {
    // Its size is not known before it is drawn: the text's width and about seven lines.
    constexpr ImVec2 kAbout{kTextWidth + 24, 170};
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float left = viewport->WorkPos.x, right = left + viewport->WorkSize.x;
    const float x = std::clamp(pointer.x, left + kAbout.x / 2,
                               std::max(left + kAbout.x / 2, right - kAbout.x / 2));
    if (pointer.y - 14 - kAbout.y >= viewport->WorkPos.y) {
        open({x, pointer.y - 14}, {0.5f, 1.0f});
    } else {
        open({x, pointer.y + 24}, {0.5f, 0.0f});
    }
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
    if (ImGui::Button("OK", {110, kBig})) {
        dontAsk_ = dontAsk_ || ticked_;
        answer = PopupAnswer::Ok;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {110, kBig})) {
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
