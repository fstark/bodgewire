#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace bw {

// A real ROM dump the user keeps in git-ignored external/roms/ (P5n, tests/data/pet_roms.md): its
// path, or nullopt when it is missing, and the test SKIPs. `name` as
// "pet/rom-1-c000.901447-01.bin".
inline std::optional<std::filesystem::path> ExternalRom(std::string_view name) {
    const std::filesystem::path path = std::filesystem::path(BW_ROOT_DIR) / "external/roms" / name;
    if (!std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }
    return path;
}

// The PET 2001's BASIC 1 (MAME's pet2001 set, basic1o) by socket, as tests/data/pet_roms.md.
struct PetRom {
    std::string_view socket;  // "UH1"
    std::string_view file;    // under external/roms/
    unsigned address;
};
inline constexpr PetRom kPetBasic1[] = {
    {"UH1", "pet/rom-1-c000.901447-01.bin", 0xC000},
    {"UH5", "pet/rom-1-c800.901447-02.bin", 0xC800},
    {"UH2", "pet/rom-1-d000.901447-03.bin", 0xD000},
    {"UH6", "pet/rom-1-d800.901447-04.bin", 0xD800},
    {"UH3", "pet/rom-1-e000.901447-05.bin", 0xE000},
    {"UH4", "pet/rom-1-f000.901447-06.bin", 0xF000},
    {"UH7", "pet/rom-1-f800.901447-07.bin", 0xF800},
};

inline constexpr std::string_view kNoPetRoms = "no external/roms/pet: see tests/data/pet_roms.md";

}  // namespace bw
