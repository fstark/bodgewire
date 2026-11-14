#include "asm/asm6502.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/mos6502_core.hpp"
#include "parts/rom_contents.hpp"

using namespace bw;

namespace {

constexpr std::string_view kHeader = "  .rom $F000, $1000\n  .org $F000\n";

// The bytes a program assembles to from $F000, up to the last one written.
std::vector<std::uint8_t> Bytes(std::string_view body) {
    const auto rom = Assemble6502(std::string(kHeader) + std::string(body), "t.asm");
    if (!rom) {
        FAIL(rom.error());
    }
    std::size_t end = rom->words.size();
    while (end > 0 && rom->words[end - 1] == 0xFF) {
        --end;
    }
    return {rom->words.begin(), rom->words.begin() + static_cast<std::ptrdiff_t>(end)};
}

std::string Error(std::string_view body) {
    const auto rom = Assemble6502(std::string(kHeader) + std::string(body), "t.asm");
    REQUIRE_FALSE(rom.has_value());
    return rom.error();
}

using Bytes_ = std::vector<std::uint8_t>;

std::string SampleOperand(Mos6502Operand form) {
    using enum Mos6502Operand;
    switch (form) {
    case None:
        return "";
    case Accumulator:
        return "A";
    case Immediate:
        return "#$12";
    case ZeroPage:
        return "$12";
    case ZeroPageX:
        return "$12,X";
    case ZeroPageY:
        return "$12,Y";
    case Absolute:
        return "$1234";
    case AbsoluteX:
        return "$1234,X";
    case AbsoluteY:
        return "$1234,Y";
    case Indirect:
        return "($1234)";
    case IndirectX:
        return "($12,X)";
    case IndirectY:
        return "($12),Y";
    case Relative:
        return "*+5";
    }
    return "";
}

}  // namespace

TEST_CASE("asm: every documented opcode assembles to itself", "[asm]") {
    using enum Mos6502Operand;
    for (int code = 0; code < 256; ++code) {
        const Mos6502Instruction i = Mos6502Decode(static_cast<std::uint8_t>(code));
        if (!i.documented) {
            continue;
        }
        const std::string line = "  " + std::string(i.mnemonic) + " " + SampleOperand(i.operand);
        INFO(line);
        const Bytes_ bytes = Bytes(line + "\n");
        REQUIRE_FALSE(bytes.empty());
        CHECK(bytes[0] == code);
        switch (i.operand) {
        case None:
        case Accumulator:
            CHECK(bytes.size() == 1);
            break;
        case Absolute:
        case AbsoluteX:
        case AbsoluteY:
        case Indirect:
            CHECK(bytes == Bytes_{static_cast<std::uint8_t>(code), 0x34, 0x12});
            break;
        case Relative:
            CHECK(bytes == Bytes_{static_cast<std::uint8_t>(code), 0x03});
            break;
        default:
            CHECK(bytes == Bytes_{static_cast<std::uint8_t>(code), 0x12});
        }
    }
}

TEST_CASE("asm: zero page for a known small value, absolute for a forward one", "[asm]") {
    CHECK(Bytes("  lda $12\n") == Bytes_{0xA5, 0x12});
    CHECK(Bytes("  lda $0012\n") == Bytes_{0xA5, 0x12});  // the value decides, not the digits
    CHECK(Bytes("  lda $123\n") == Bytes_{0xAD, 0x23, 0x01});
    CHECK(Bytes("v = 3\n  lda v,x\n") == Bytes_{0xB5, 0x03});
    CHECK(Bytes("  lda v\nv = 3\n") == Bytes_{0xAD, 0x03, 0x00});  // unknown in pass 1
    CHECK(Bytes("  ldx $12,y\n  ldx $1234,y\n") == Bytes_{0xB6, 0x12, 0xBE, 0x34, 0x12});
    CHECK(Bytes("  jmp $0012\n") == Bytes_{0x4C, 0x12, 0x00});  // no zero-page JMP
    CHECK(Bytes("  stx v,y\nv = 5\n") == Bytes_{0x96, 0x05});   // only zp,Y exists
    CHECK(Bytes("  asl\n  asl a\n  ASL A\n") == Bytes_{0x0A, 0x0A, 0x0A});
}

TEST_CASE("asm: branches", "[asm]") {
    CHECK(Bytes("back: nop\n  bne back\n  beq fwd\n  nop\nfwd: rts\n") ==
          Bytes_{0xEA, 0xD0, 0xFD, 0xF0, 0x01, 0xEA, 0x60});
    CHECK(Bytes("  bcc *\n") == Bytes_{0x90, 0xFE});
    std::string far = "top: nop\n";
    for (int i = 0; i < 130; ++i) {
        far += "  nop\n";
    }
    far += "  bne top\n";
    CHECK(Error(far).find("t.asm line 134: branch out of range") == 0);
}

TEST_CASE("asm: labels, local labels and constants", "[asm]") {
    CHECK(Bytes("one: ldx #2\n@l: dex\n  bne @l\ntwo: ldy #2\n@l: dey\n  bne @l\n  jmp one\n") ==
          Bytes_{0xA2, 0x02, 0xCA, 0xD0, 0xFD, 0xA0, 0x02, 0x88, 0xD0, 0xFD, 0x4C, 0x00, 0xF0});
    CHECK(Bytes("K = $40\nW = K * 2 + 1\n  lda #W\n") == Bytes_{0xA9, 0x81});
    CHECK(Bytes("b:\n  jmp b\n") == Bytes_{0x4C, 0x00, 0xF0});
    CHECK(Error("a: nop\na: nop\n") == "t.asm line 4: 'a' defined twice");
    CHECK(Error("  jmp nowhere\n") == "t.asm line 3: undefined symbol 'nowhere'");
    CHECK(Error("@l: nop\n").find("before any label") != std::string::npos);
    CHECK(Error("K = later\nlater: nop\n").find("needs a value known") != std::string::npos);
}

TEST_CASE("asm: expressions", "[asm]") {
    CHECK(Bytes("  .byte 1 + 2 * 3, (1 + 2) * 3, 7 - 2 - 1, 100 / 7\n") == Bytes_{7, 9, 4, 14});
    CHECK(Bytes("  .byte 1 << 4 | 1, $F0 & $3C, $0F ^ $FF, 1 + 1 << 2, $80 >> 3\n") ==
          Bytes_{0x11, 0x30, 0xF0, 0x08, 0x10});
}

TEST_CASE("asm: data directives and .org", "[asm]") {
    CHECK(Bytes("  .byte <$1234, >$1234, -1, ~0 & $FF, %1010, 'A', ';'\n") ==
          Bytes_{0x34, 0x12, 0xFF, 0xFF, 0x0A, 0x41, 0x3B});
    CHECK(Bytes("  .byte \"Hi \\\"x\\\"\", 0\n") == Bytes_{'H', 'i', ' ', '"', 'x', '"', 0});
    CHECK(Bytes("  .word $1234, *\n") == Bytes_{0x34, 0x12, 0x02, 0xF0});
    CHECK(Bytes("  nop\n  .org $F004\n  nop\n") == Bytes_{0xEA, 0xFF, 0xFF, 0xFF, 0xEA});
}

TEST_CASE("asm: errors name the line", "[asm]") {
    CHECK(Error("  foo\n") == "t.asm line 3: unknown instruction 'foo'");
    CHECK(Error("  ldx ($12,x)\n") == "t.asm line 3: LDX has no (zp,X) form");
    CHECK(Error("  jmp\n") == "t.asm line 3: JMP has no implied form");
    CHECK(Error("  lda #256\n") == "t.asm line 3: immediate value 256 out of range");
    CHECK(Error("  lda ($123),y\n") == "t.asm line 3: (zp),Y value 291 out of range");
    CHECK(Error("  nop\n  .org $F000\n  nop\n") == "t.asm line 5: $F000 written twice");
    CHECK(Error("  .org $E000\n  nop\n").find("outside the .rom window $F000-$FFFF") !=
          std::string::npos);
    CHECK(Error("  .byte 1 /\n").find("t.asm line 3: missing value") == 0);
    CHECK(Error("  .rom 0, 16\n") == "t.asm line 3: .rom given twice");
    CHECK(Error("  .bogus\n") == "t.asm line 3: unknown directive '.bogus'");
    const auto noRom = Assemble6502("  .org 0\n  nop\n", "n.asm");
    REQUIRE_FALSE(noRom.has_value());
    CHECK(noRom.error() == "n.asm line 2: no .rom BASE, SIZE before the first byte");
    const auto badSize = Assemble6502("  .rom $F000, 1000\n", "n.asm");
    REQUIRE_FALSE(badSize.has_value());
    CHECK(badSize.error().find("power of two") != std::string::npos);
}

TEST_CASE("asm: the .rom text reads back as the same ROM", "[asm]") {
    const auto rom = Assemble6502(
        "; a test\n  .rom $FF00, 256\n\n  .org $FF00\nstart: lda #1 ; one\n  jmp start\n"
        "  .org $FFFC\n  .word start\n",
        "p.asm");
    REQUIRE(rom.has_value());
    CHECK(rom->base == 0xFF00);
    CHECK(rom->text ==
          "; Generated by bw asm from p.asm: do not edit. Edit the .asm and run bw asm again.\n"
          "; ; a test\n"
          ";   .rom $FF00, 256\n"
          "\n"
          ";   .org $FF00\n"
          "@000 A9 01     ; FF00  start: lda #1 ; one\n"
          "@002 4C 00 FF  ; FF02    jmp start\n"
          ";   .org $FFFC\n"
          "@0FC 00 FF     ; FFFC    .word start\n");
    const auto words = ParseRomContents(rom->text, "p.rom", 256, 8);
    REQUIRE(words.has_value());
    CHECK(*words == rom->words);
}

TEST_CASE("asm: .checksum makes the ROM sum to 0", "[asm]") {
    const auto rom = Assemble6502("  .rom $F000, $400\n  .org $F000\n  lda #$12\n  .checksum\n"
                                  "  jmp $F000\n  .org $F3FC\n  .word $F000\n",
                                  "c.asm");
    REQUIRE(rom.has_value());
    unsigned sum = 0;
    for (const std::uint8_t w : rom->words) {
        sum += w;
    }
    CHECK((sum & 0xFF) == 0);
    // The byte is the one the .rom text carries.
    const auto words = ParseRomContents(rom->text, "c.rom", 0x400, 8);
    REQUIRE(words.has_value());
    CHECK(*words == rom->words);
    CHECK(rom->text.find(std::format("@002 {:02X}", rom->words[2])) != std::string::npos);
    // Without one, the bytes are as written.
    CHECK(Bytes("  nop\n") == Bytes_{0xEA});
    CHECK(Error("  .checksum 5\n") == "t.asm line 3: .checksum takes no value");
    CHECK(Error("  .checksum\n  .checksum\n") == "t.asm line 4: .checksum given twice");
    const auto early = Assemble6502("  .org 0\n  .checksum\n", "n.asm");
    REQUIRE_FALSE(early.has_value());
    CHECK(early.error() == "n.asm line 2: no .rom BASE, SIZE before the first byte");
}

TEST_CASE("asm: every ROM program matches its committed .rom", "[asm]") {
    const std::filesystem::path dir = BW_ROM_DIR;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".asm") {
            continue;
        }
        INFO(entry.path().filename().string());
        const auto rom = Assemble6502File(entry.path());
        REQUIRE(rom.has_value());
        std::ifstream in(std::filesystem::path(entry.path()).replace_extension(".rom"),
                         std::ios::binary);
        REQUIRE(in);
        std::ostringstream committed;
        committed << in.rdbuf();
        CHECK(committed.str() == rom->text);
    }
}
