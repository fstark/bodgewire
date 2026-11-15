// The real PET ROMs, when the user has placed them (tests/data/pet_roms.md): each reads as a
// 2316B's 2048 bytes. Skipped without them.

#include "external_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include "parts/rom_contents.hpp"

using namespace bw;

TEST_CASE("external roms: PET BASIC 1 dumps read as 2 KB ROMs", "[netlist]") {
    for (const PetRom& rom : kPetBasic1) {
        INFO(rom.file);
        const auto path = ExternalRom(rom.file);
        if (!path) {
            SKIP(kNoPetRoms);
        }
        const auto words = ReadRomContents(*path, 2048, 8);
        INFO((words ? std::string() : words.error()));
        CHECK(words.has_value());
    }
    // The reset vector, in 901447-07 at $FFFC: $FD38 (BASIC 1).
    const auto top = ReadRomContents(*ExternalRom(kPetBasic1[6].file), 2048, 8);
    REQUIRE(top.has_value());
    CHECK((*top)[0x7FC] == 0x38);
    CHECK((*top)[0x7FD] == 0xFD);
}
