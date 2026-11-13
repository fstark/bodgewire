#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "parts/mos6502_core.hpp"

namespace bw::test {

// One single-instruction case in the compact format of tests/data/6502 (MOS6502_DESIGN, Test
// data): registers and RAM before and after, and the bus of each cycle.
struct Mos6502Case {
    struct Cell {
        std::uint16_t address;
        std::uint8_t value;
    };
    struct Cycle {
        std::uint16_t address;
        std::uint8_t value;
        bool read;
    };
    std::string name;
    Mos6502Registers initial, final;
    std::vector<Cell> initialRam, finalRam;
    std::vector<Cycle> cycles;
};

// The cases of one file; the error names the file and line.
std::expected<std::vector<Mos6502Case>, std::string>
ReadMos6502Cases(const std::filesystem::path& path);

// Runs the case on a fresh core over a 64 KiB array: nothing if it matches, else the first
// difference.
std::optional<std::string> RunMos6502Case(const Mos6502Case& c);

}  // namespace bw::test
