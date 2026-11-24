#include "ui/thank_you_card.hpp"

#include <cfloat>

#include <imgui.h>

#include "ui/brief_window.hpp"

namespace bw {

namespace {

constexpr const char* kId = "##thank you";
constexpr float kWidth = 400;
constexpr float kPad = 20;
constexpr float kHeadingScale = 1.6f;
constexpr ImU32 kDim = IM_COL32(0, 0, 0, 60);
constexpr ImU32 kButton = IM_COL32(228, 224, 212, 255);
constexpr ImU32 kButtonHovered = IM_COL32(212, 206, 190, 255);
constexpr ImU32 kButtonActive = IM_COL32(196, 189, 170, 255);
constexpr ImU32 kButtonBorder = IM_COL32(150, 142, 120, 255);

// The card under the window's contents: shadow, paper, a red rule under the heading at `rule`,
// blue lines below it down to `linesEnd` (above the buttons).
void DrawCard(ImDrawList& list, ImVec2 at, ImVec2 end, float rule, float line, float linesEnd) {
    list.AddRectFilled({at.x + 4, at.y + 4}, {end.x + 4, end.y + 4}, kCardShadow, 2.0f);
    list.AddRectFilled(at, end, kCardPaper, 2.0f);
    list.AddLine({at.x, rule}, {end.x, rule}, kCardRed, 1.0f);
    for (float y = rule + line; y < linesEnd; y += line) {
        list.AddLine({at.x, y}, {end.x, y}, kCardBlue, 1.0f);
    }
}

}  // namespace

CardAnswer ThankYouCard::draw(const std::string& title, const std::string* next, bool canLeave) {
    if (opening_) {
        ImGui::OpenPopup(kId);
        opening_ = false;
        shown_ = true;
    }
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport.WorkPos.x + viewport.WorkSize.x / 2,
                             viewport.WorkPos.y + viewport.WorkSize.y / 2},
                            ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSizeConstraints({kWidth, 0}, {FLT_MAX, FLT_MAX});  // wider for long titles
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, kDim);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0, 0, 0, 0));  // the card is drawn
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPad, kPad});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const bool open = ImGui::BeginPopupModal(
        kId, nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    if (!open) {
        if (shown_) {  // closed by ImGui: as Back to the Board
            shown_ = false;
            return CardAnswer::Board;
        }
        return CardAnswer::None;
    }
    const float size = ImGui::GetFontSize();
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 at = ImGui::GetWindowPos();
    const ImVec2 end{at.x + ImGui::GetWindowWidth(), at.y + ImGui::GetWindowHeight()};
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    DrawCard(*ImGui::GetWindowDrawList(), at, end, at.y + kPad + size * kHeadingScale + spacing / 2,
             line, end.y - kPad - ImGui::GetFrameHeight() - spacing);

    ImGui::PushStyleColor(ImGuiCol_Text, kNoteInk);
    ImGui::PushFont(nullptr, size * kHeadingScale);
    ImGui::TextUnformatted("Repaired!");
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::PushTextWrapPos(kWidth - kPad);
    ImGui::TextUnformatted((title + ": it works again. Thank you!").c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, kButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Border, kButtonBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    CardAnswer answer = CardAnswer::None;
    if (ImGui::Button("Back to the Board")) {
        answer = CardAnswer::Board;
    }
    if (next != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button(("Next: " + *next).c_str())) {
            answer = CardAnswer::Next;
        }
    }
    if (canLeave) {
        ImGui::SameLine();
        if (ImGui::Button("Back to the Machines")) {
            answer = CardAnswer::Machines;
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    if (answer != CardAnswer::None) {
        ImGui::CloseCurrentPopup();
        shown_ = false;
    }
    ImGui::EndPopup();
    return answer;
}

}  // namespace bw
