#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <imgui.h>

namespace bw {

enum class PopupAnswer : std::uint8_t { None, Ok, Cancel };

// A question asked at a click, OK and Cancel, mouse only (the keys go to the Machine): the
// power-off tools' with the power on (P6o), with a "Don't ask again" box that counts with OK
// only, for the bench, every tool, never saved; going back to the launcher with work on the
// Board (P7a). A click elsewhere cancels; ImGui keeps that click from acting under the popup.
class QuestionPopup {
public:
    // `id`: ImGui's ("##power cycle"); `dontAskBox`: the box offered.
    QuestionPopup(const char* id, std::string text, bool dontAskBox);
    // At `at` from the next draw, `pivot` ImGui's (0, 0: its top-left at `at`; 0, 1: opening
    // upwards from it).
    void open(ImVec2 at, ImVec2 pivot = {0, 0});
    // Above `pointer` (P8f: a finger would cover it), kept inside the main window; under it
    // near the window's top.
    void openAbove(ImVec2 pointer);
    // Each frame, last. Cancel also when closed by a click elsewhere.
    PopupAnswer draw();
    bool dontAsk() const { return dontAsk_; }

private:
    const char* id_;
    std::string text_;
    bool dontAskBox_;
    std::optional<ImVec2> opening_;
    ImVec2 pivot_{};
    bool shown_ = false;    // open: a close by ImGui is a cancel
    bool ticked_ = false;   // the box, for this question
    bool dontAsk_ = false;  // the box ticked with OK
};

}  // namespace bw
