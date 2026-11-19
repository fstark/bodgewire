#pragma once

#include <span>
#include <string>
#include <vector>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/manual_view.hpp"
#include "view/binder.hpp"

namespace bw {

// The binder's window ID, for the shelf to bring it to the front.
inline constexpr const char* kManualWindowName = "###manuals";
// --binder-height's range, of the binder's height for its width (1: its proportions).
inline constexpr float kMinStretch = 0.7f, kMaxStretch = 2.5f;

// The manuals binder (P6e): a drawn ring binder with four dividers, the User Manual, the Service
// Manual, the Datasheets (the hovered or selected chip's page, else an index of the Machine's
// Parts) and the Repairs log. Out at the User Manual; a chip selected while it is out turns it to
// that chip's page.
class ManualWindows {
public:
    void draw(bool& open, const Manuals& manuals, BoardView& board, const LoadedCircuit& design,
              std::span<const std::string> log);
    void show(Divider divider) { binder_.show(divider); }  // --manual: the divider it comes out at
    // --binder-height: its first height for its width, 1 its proportions (P6o: resized freely).
    void setStretch(float stretch) { stretch_ = stretch; }

private:
    void tabs(bool board);
    void datasheets(const Manuals& manuals, BoardView& board, const LoadedCircuit& design);
    TargetHits index(const Manuals& manuals, const BoardScene* scene);

    Binder binder_;
    ManualView user_, service_, sheet_;
    ChipPanel chip_;
    std::vector<IndexEntry> index_;
    bool indexed_ = false;
    float stretch_ = 1;
};

}  // namespace bw
