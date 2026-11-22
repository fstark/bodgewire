#pragma once

#include <filesystem>
#include <string_view>

#include "../paths.hpp"

namespace bw {

// The PET 2001's BASIC 1 (MAME's pet2001 set, basic1o), the BW-2001's ROMs, by socket
// (tests/data/pet_roms.md).
struct PetRom {
    std::string_view socket;  // "UH1"
    std::string_view file;    // in machines/130-bw2001/roms/
    unsigned address;
};
inline constexpr PetRom kPetBasic1[] = {
    {"UH1", "901447-01.bin", 0xC000}, {"UH5", "901447-02.bin", 0xC800},
    {"UH2", "901447-03.bin", 0xD000}, {"UH6", "901447-04.bin", 0xD800},
    {"UH3", "901447-05.bin", 0xE000}, {"UH4", "901447-06.bin", 0xF000},
    {"UH7", "901447-07.bin", 0xF800},
};

inline std::filesystem::path PetRomPath(const PetRom& rom) {
    return test::RomPath("bw2001", rom.file);
}

}  // namespace bw
