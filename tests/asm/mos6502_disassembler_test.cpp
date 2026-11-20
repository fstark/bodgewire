#include "asm/mos6502_disassembler.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "asm/assembler.hpp"
#include "parts/mos6502_core.hpp"

using namespace bw;

TEST_CASE("disassembler 6502: every documented opcode assembles back to its bytes", "[asm]") {
    int documented = 0;
    for (unsigned op = 0; op < 256; ++op) {
        const std::array<std::uint8_t, 3> bytes{static_cast<std::uint8_t>(op), 0x34, 0x12};
        const auto text = Disassemble6502(bytes, 0x0200);
        INFO(op);
        REQUIRE(text.has_value() == Mos6502Decode(bytes[0]).documented);
        if (!text) {
            continue;
        }
        ++documented;
        INFO(text->text);
        const auto rom = Assemble("  .rom $0200, $100\n  .org $0200\n  " + text->text + "\n", "t");
        REQUIRE(rom.has_value());
        CHECK(std::vector<std::uint8_t>(rom->words.begin(),
                                        rom->words.begin() + static_cast<long>(text->length)) ==
              std::vector<std::uint8_t>(bytes.begin(),
                                        bytes.begin() + static_cast<long>(text->length)));
    }
    CHECK(documented == 151);
}

TEST_CASE("disassembler 6502: forms, branches and short buffers", "[asm]") {
    const auto text = [](std::vector<std::uint8_t> bytes, std::uint16_t at = 0x1C00) {
        const auto d = Disassemble6502(bytes, at);
        return d ? d->text : std::string("-");
    };
    CHECK(text({0xA9, 0x00}) == "LDA #$00");
    CHECK(text({0xAD, 0x34, 0x00}) == "LDA $0034");  // absolute on zero page: 4 digits
    CHECK(text({0xB1, 0xFA}) == "LDA ($FA),Y");
    CHECK(text({0x0A}) == "ASL A");
    CHECK(text({0x6C, 0xFA, 0x17}) == "JMP ($17FA)");
    CHECK(text({0xD0, 0xFE}) == "BNE $1C00");          // a branch to itself
    CHECK(text({0xD0, 0x80}, 0x0010) == "BNE $FF92");  // wraps round 64K
    CHECK(text({0xFF, 0xFF, 0xFF}) == "-");            // undocumented: data
    CHECK(text({0xAD, 0x34}) == "-");                  // too few bytes
    CHECK(text({}) == "-");
    CHECK(Disassemble6502(std::vector<std::uint8_t>{0x20, 0x00, 0x1C}, 0)->length == 3);
}
