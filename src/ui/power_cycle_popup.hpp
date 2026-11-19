#pragma once

#include <cstdint>
#include <optional>

#include <imgui.h>

namespace bw {

enum class PopupAnswer : std::uint8_t { None, Ok, Cancel };

// The question a power-off tool asks with the power on (P6o): "This tool needs the power off…",
// OK and Cancel, mouse only (the keys go to the Machine), a "Don't ask again" box that counts
// with OK only, for the run, every tool, never saved. A click elsewhere cancels; ImGui keeps that
// click from acting under the popup.
class PowerCyclePopup {
public:
    void open(ImVec2 at);  // at the click, from the next draw
    // Each frame, last. Cancel also when closed by a click elsewhere.
    PopupAnswer draw();
    bool dontAsk() const { return dontAsk_; }

private:
    std::optional<ImVec2> opening_;
    bool shown_ = false;    // open: a close by ImGui is a cancel
    bool ticked_ = false;   // the box, for this question
    bool dontAsk_ = false;  // the box ticked with OK
};

}  // namespace bw
