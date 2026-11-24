#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "netlist/diagnostic.hpp"

namespace bw {

// What the launcher lists (P7a).
struct PuzzleEntry {
    std::filesystem::path file;  // machines/NNN-name/puzzles/NN-name.bwp
    std::string title, brief;
};

struct MachineEntry {
    std::filesystem::path dir;         // machines/NNN-name
    std::filesystem::path netlist;     // its one .bwn
    std::string name;                  // the User Manual's title, else the directory's name
    std::string line;                  // the User Manual's first paragraph, plain; "" without one
    std::vector<PuzzleEntry> puzzles;  // in file name order (NN-, easiest first)
};

struct MachineList {
    std::vector<MachineEntry> machines;  // in directory name order (NNN-)
    std::vector<Diagnostic> problems;    // a Puzzle that does not load, two netlists: left out
};

// Each subdirectory of `machinesDir` with exactly one .bwn is a Machine; others (a .DS_Store, a
// directory without a netlist) are skipped silently, two netlists are a problem. Its Puzzles are
// puzzles/*.bwp read by ReadPuzzleFile (no puzzles/: none); one that fails, or names another
// Machine than its netlist's stem, is a problem, left out. Never throws: a missing or unreadable
// directory lists nothing.
MachineList ListMachines(const std::filesystem::path& machinesDir);

// `from`, then each parent: the first <dir>/machines holding a Machine (a subdirectory with a
// .bwn). nullopt at the root. Never throws.
std::optional<std::filesystem::path> FindMachinesDir(const std::filesystem::path& from);

// The Machine whose directory is MachineDir(file) (a .bwn or a .bwp), compared as weakly
// canonical paths.
std::optional<std::size_t> MachineIndexOf(const MachineList& list,
                                          const std::filesystem::path& file);

// An entry of the list: a Machine's netlist (the working Machine) or one of its Puzzles.
struct EntryRef {
    std::size_t machine;
    std::optional<std::size_t> puzzle;  // nullopt: the working Machine
};
// The entry that is `file`, compared as weakly canonical paths: --visit's check.
std::optional<EntryRef> EntryOf(const MachineList& list, const std::filesystem::path& file);

// The Puzzle after `file` among its Machine's, as listed (P8c's Thank-you Card); nullptr for the
// Machine's last Puzzle, its working Machine or a file not in the list.
const PuzzleEntry* NextPuzzle(const MachineList& list, const std::filesystem::path& file);

}  // namespace bw
