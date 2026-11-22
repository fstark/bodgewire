#pragma once

// Repository files for tests, by name: a Machine's files under machines/NNN-name/ found by its
// netlist's name ("counter"), a Puzzle by its name without the order number ("dark_led").

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bw::test {

inline std::filesystem::path RootDir() { return BW_ROOT_DIR; }

// machines/NNN-name of the Machine whose netlist is <machine>.bwn.
inline std::filesystem::path MachineDir(std::string_view machine) {
    const std::string file = std::string(machine) + ".bwn";
    for (const auto& entry : std::filesystem::directory_iterator(RootDir() / "machines")) {
        if (std::filesystem::exists(entry.path() / file)) {
            return entry.path();
        }
    }
    throw std::runtime_error("no Machine " + std::string(machine));
}

inline std::filesystem::path NetlistPath(std::string_view machine) {
    return MachineDir(machine) / (std::string(machine) + ".bwn");
}

inline std::filesystem::path BoardPath(std::string_view machine) {
    return MachineDir(machine) / "board" / (std::string(machine) + ".kicad_pcb");
}

inline std::filesystem::path RomPath(std::string_view machine, std::string_view file) {
    return MachineDir(machine) / "roms" / std::string(file);
}

// A Machine's own debug firmware (the BW-2001's, --roms machines/130-bw2001/debug-roms).
inline std::filesystem::path DebugRomDir(std::string_view machine) {
    return MachineDir(machine) / "debug-roms";
}

inline std::filesystem::path DebugRomPath(std::string_view machine, std::string_view file) {
    return DebugRomDir(machine) / std::string(file);
}

// bench/<file>: the benchmark netlists and their ROMs.
inline std::filesystem::path BenchPath(std::string_view file) {
    return RootDir() / "bench" / std::string(file);
}

// machines/*/puzzles/NN-<name>.bwp, `name` with '_' or '-' ("dark_led", "dark-led").
inline std::filesystem::path PuzzlePath(std::string_view name) {
    std::string wanted(name);
    for (char& c : wanted) {
        if (c == '_') {
            c = '-';
        }
    }
    if (wanted.ends_with(".bwp")) {
        wanted.resize(wanted.size() - 4);
    }
    for (const auto& machine : std::filesystem::directory_iterator(RootDir() / "machines")) {
        const auto puzzles = machine.path() / "puzzles";
        if (!std::filesystem::is_directory(puzzles)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(puzzles)) {
            const std::string stem = entry.path().stem().string();
            const auto dash = stem.find('-');
            if (dash != std::string::npos && stem.substr(dash + 1) == wanted) {
                return entry.path();
            }
        }
    }
    throw std::runtime_error("no Puzzle " + std::string(name));
}

}  // namespace bw::test
