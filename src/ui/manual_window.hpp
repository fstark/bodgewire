#pragma once

#include <span>
#include <string>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"
#include "ui/chip_panel.hpp"
#include "ui/manual_view.hpp"

namespace bw {

// The manuals window's ID (its title is "Manuals"), for the shelf to bring it to the front.
inline constexpr const char* kManualWindowName = "Manuals###manuals";

// The floating manuals window (P6d, until P6e's binder): the User Manual, and with a Board the
// Chip (the selected or hovered chip: role, pinout, Datasheet) and the Repairs log, as tabs.
class ManualWindows {
public:
    void draw(bool& open, const Manuals& manuals, BoardView& board, const LoadedCircuit& design,
              std::span<const std::string> log);
    // The next draw shows the Chip tab (`--manual chip`).
    void showChip() { showChip_ = true; }

private:
    ManualView user_;
    ChipPanel chip_;
    bool showChip_ = false;
};

}  // namespace bw
