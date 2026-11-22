#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include "machine/machine_list.hpp"

namespace bw {

// The launcher (P7a): the Machines on the left, the selected one's entries on the right (the
// working Machine, then its Puzzles). Plain ImGui; its look is P7b's.
class Launcher {
public:
    void setList(MachineList list);    // read anew each time it shows; the selection clamped
    void select(std::size_t machine);  // the Machine just left (clamped)
    void setError(std::string text);   // "Could not load …: see the terminal", in red
    const MachineList& list() const { return list_; }
    // Each frame, inside a window filling the main one: the file clicked (a .bwn or a .bwp).
    std::optional<std::filesystem::path> draw();

private:
    MachineList list_;
    std::size_t selected_ = 0;
    bool scroll_ = false;  // the selected name brought into view at the next draw
    std::string error_;
};

}  // namespace bw
