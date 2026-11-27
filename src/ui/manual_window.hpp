#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/contents_page.hpp"
#include "ui/controls.hpp"
#include "ui/manual_view.hpp"
#include "view/binder.hpp"

namespace bw {

// The binder's window ID, for the shelf to bring it to the front.
inline constexpr const char* kManualWindowName = "###manuals";

// The manuals binder (P6e): a drawn ring binder with four dividers, the User Manual, the Service
// Manual, the Datasheets (the hovered or selected chip's page, else an index of the Machine's
// Parts) and the Repairs log; a fifth, Contents, once a memory chip's window was clicked (P6p);
// a sixth, Give up, on a Random fault's bench (P8d): its page shows the Fault once asked.
// Out at the User Manual; a chip selected while it is out turns it to that chip's page.
class ManualWindows {
public:
    // `machine`: the Contents page's words; null without a Machine (no Board, no Contents).
    // `fault`: a Random fault's `fault …` line, for the Give up page; null on any other bench.
    void draw(bool& open, const Manuals& manuals, BoardView& board, const LoadedCircuit& design,
              std::span<const std::string> log, const Machine* machine,
              const std::string* fault = nullptr);
    void showFault() { faultShown_ = true; }  // --show-fault: the Give up page as after its button
    void show(Divider divider) { binder_.show(divider); }  // --manual: the divider it comes out at
    // --binder-height: its first height for its width, 1 its proportions (P6o: resized freely).
    void setStretch(float stretch) { stretch_ = stretch; }
    // A memory chip's window clicked, or --contents (P6p): the Contents divider at that chip, at
    // `view` (else the chip's default the first time, then the player's last).
    void openContents(const BoardScene& scene, std::size_t component,
                      std::optional<ContentsView> view = std::nullopt);
    // Called with the new size when the aA key changes the reading size, to keep it.
    void setReadingSizeSaver(std::function<void(int)> save) { saveReadingSize_ = std::move(save); }
    void setContentsDefaults(ContentsDefaults defaults) {
        contents_.setDefaults(std::move(defaults));
    }

private:
    void tabs(bool board, bool giveUp);
    void readingSizeKey();
    void giveUp(const std::string& fault);
    void datasheets(const Manuals& manuals, BoardView& board, const LoadedCircuit& design);
    TargetHits index(const BoardScene* scene);

    Binder binder_;
    ManualView user_, service_;
    ChipPanel chip_;
    ContentsPage contents_;
    std::vector<IndexEntry> index_;
    bool indexed_ = false;
    std::optional<std::size_t> offered_;  // the selection last looked at for its Contents
    DragScroll pageScroll_;               // a drag on the page scrolls it (P8f)
    float stretch_ = 1;
    std::function<void(int)> saveReadingSize_;
    bool faultShown_ = false;   // the Give up page's button pressed: for the bench
    bool faultCopied_ = false;  // its line clicked
};

}  // namespace bw
