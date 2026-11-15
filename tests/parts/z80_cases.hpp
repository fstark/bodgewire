#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "parts/z80_core.hpp"

namespace bw::test {

// One single-instruction case in the compact format of tests/data/z80 (Z80_DESIGN, Test data):
// registers and RAM before and after, the bus at each T-state, the ports read and written.
struct Z80Case {
    struct Cell {
        std::uint16_t address;
        std::uint8_t value;
    };
    struct TState {
        int address;                // -1: none
        int data;                   // -1: none
        std::array<char, 4> flags;  // Harte's "rwmi", '-' where not set
    };
    struct Port {
        std::uint16_t address;
        std::uint8_t value;
        bool read;
    };
    std::string name;
    Z80Registers initial, final;
    std::vector<Cell> initialRam, finalRam;
    std::vector<TState> tStates;
    std::vector<Port> ports;
};

// The cases of one file; the error names the file and line.
std::expected<std::vector<Z80Case>, std::string> ReadZ80Cases(const std::filesystem::path& path);

// Runs the case on a fresh core over a 64 KiB array: nothing if it matches, else the first
// difference.
std::optional<std::string> RunZ80Case(const Z80Case& c);

}  // namespace bw::test
