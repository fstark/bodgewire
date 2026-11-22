#include "asm/assembler.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "parts/mos6502_core.hpp"
#include "parts/rom_contents.hpp"

using namespace bw;

namespace {

constexpr std::string_view kHeader = "  .rom $F000, $1000\n  .org $F000\n";

// The bytes a program assembles to from $F000, up to the last one written.
std::vector<std::uint8_t> Bytes(std::string_view body) {
    const auto rom = Assemble(std::string(kHeader) + std::string(body), "t.asm");
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
    const auto rom = Assemble(std::string(kHeader) + std::string(body), "t.asm");
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
    const auto noRom = Assemble("  .org 0\n  nop\n", "n.asm");
    REQUIRE_FALSE(noRom.has_value());
    CHECK(noRom.error() == "n.asm line 2: no .rom BASE, SIZE before the first byte");
    const auto badSize = Assemble("  .rom $F000, 1000\n", "n.asm");
    REQUIRE_FALSE(badSize.has_value());
    CHECK(badSize.error().find("power of two") != std::string::npos);
}

TEST_CASE("asm: .cpu chooses the instruction set, the 6502 without it", "[asm]") {
    const std::string body = "  .org $F000\n  lda #1\n  jmp $F000\n";
    const auto plain = Assemble("  .rom $F000, $1000\n" + body, "c.asm");
    const auto given = Assemble("  .cpu 6502\n  .rom $F000, $1000\n" + body, "c.asm");
    REQUIRE(plain.has_value());
    REQUIRE(given.has_value());
    CHECK(plain->words == given->words);
    CHECK(Error("  .cpu 8080\n") == "t.asm line 3: unknown CPU '8080'");
    CHECK(Error("  nop\n  .cpu z80\n") == "t.asm line 4: .cpu must come before any instruction");
    const auto twice = Assemble("  .cpu 6502\n  .cpu z80\n  .rom 0, 16\n", "c.asm");
    REQUIRE_FALSE(twice.has_value());
    CHECK(twice.error() == "c.asm line 2: .cpu given twice");
    const auto z80 = Assemble("  .cpu z80\n  .rom 0, 16\n  .org 0\n  lda #1\n", "c.asm");
    REQUIRE_FALSE(z80.has_value());
    CHECK(z80.error() == "c.asm line 4: unknown instruction 'lda'");
}

TEST_CASE("asm: the .rom text reads back as the same ROM", "[asm]") {
    const auto rom =
        Assemble("; a test\n  .rom $FF00, 256\n\n  .org $FF00\nstart: lda #1 ; one\n  jmp start\n"
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
    const auto rom = Assemble("  .rom $F000, $400\n  .org $F000\n  lda #$12\n  .checksum\n"
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
    CHECK(Error("  .checksum 5\n") == "t.asm line 3: .checksum takes nothing or BASE, SIZE");
    CHECK(Error("  .checksum\n  .checksum\n") == "t.asm line 4: .checksum given twice");
    const auto early = Assemble("  .org 0\n  .checksum\n", "n.asm");
    REQUIRE_FALSE(early.has_value());
    CHECK(early.error() == "n.asm line 2: no .rom BASE, SIZE before the first byte");
}

TEST_CASE("asm: .checksum BASE, SIZE makes each window sum to 0", "[asm]") {
    const auto rom = Assemble("  .rom $F000, $1000\n  .org $F000\n  lda #$12\n"
                              "  .org $F7FF\n  .checksum $F000, $800\n"
                              "  .org $F800\n  ldx #$34\n  .checksum $F800, $800\n"
                              "  .org $FFFC\n  .word $F800\n",
                              "w.asm");
    REQUIRE(rom.has_value());
    for (const std::size_t half : {0u, 0x800u}) {
        unsigned sum = 0;
        for (std::size_t i = half; i < half + 0x800; ++i) {
            sum += rom->words[i];
        }
        CHECK((sum & 0xFF) == 0);
    }
    const auto words = ParseRomContents(rom->text, "w.rom", 0x1000, 8);
    REQUIRE(words.has_value());
    CHECK(*words == rom->words);

    CHECK(Error("  .checksum $F000\n") == "t.asm line 3: .checksum takes nothing or BASE, SIZE");
    CHECK(Error("  .checksum $F800, $800\n") == "t.asm line 3: .checksum byte outside its window");
    CHECK(Error("  .checksum $F000, $2000\n") == "t.asm line 3: .checksum window outside .rom");
    CHECK(Error("  .checksum $F000, $800\n  .checksum $F000, $10\n") ==
          "t.asm line 4: .checksum windows overlap");
    CHECK(Error("  .checksum $F000, $800\n  .checksum\n") == "t.asm line 4: .checksum given twice");
    CHECK(Error("  .checksum\n  .checksum $F000, $800\n") == "t.asm line 4: .checksum given twice");
}

TEST_CASE("asm: .include", "[asm]") {
    const auto dir = std::filesystem::temp_directory_path() / "bw_asm_include_test";
    std::filesystem::create_directories(dir / "sub");
    const auto write = [&](const std::filesystem::path& path, std::string_view text) {
        std::ofstream(dir / path) << text;
    };
    write("b.inc", "VALUE = $42\nroutine: lda #VALUE\n  rts\n");
    write("a.asm", "  .rom $F000, $1000\n  .org $F000\n  .include \"b.inc\"\n  jsr routine\n");
    const auto rom = AssembleFile(dir / "a.asm");
    REQUIRE(rom.has_value());
    CHECK(Bytes("VALUE = $42\nroutine: lda #VALUE\n  rts\n  jsr routine\n") ==
          Bytes_(rom->words.begin(), rom->words.begin() + 6));
    CHECK(rom->text.find("; Generated by bw asm from a.asm") != std::string::npos);
    CHECK(rom->text.find("routine: lda #VALUE") != std::string::npos);

    // Resolved against the including file's own directory.
    write("sub/c.inc", "  .include \"d.inc\"\n");
    write("sub/d.inc", "  nop\n");
    write("n.asm", "  .rom $F000, $1000\n  .org $F000\n  .include \"sub/c.inc\"\n");
    const auto nested = AssembleFile(dir / "n.asm");
    REQUIRE(nested.has_value());
    CHECK(nested->words[0] == 0xEA);

    // An error in an included file names that file and its line.
    write("bad.inc", "  nop\n  bogus\n");
    write("e.asm", "  .rom $F000, $1000\n  .org $F000\n  .include \"bad.inc\"\n");
    const auto bad = AssembleFile(dir / "e.asm");
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().starts_with("bad.inc line 2: "));

    write("m.asm", "  .include \"missing.inc\"\n");
    CHECK(AssembleFile(dir / "m.asm").error() == "m.asm line 1: cannot read 'missing.inc'");
    write("loop.inc", "  .include \"loop.inc\"\n");
    CHECK(AssembleFile(dir / "loop.inc").error() == "loop.inc line 1: .include nested too deep");
    CHECK(Error("  .include \"b.inc\"\n") == "t.asm line 3: .include needs a file on disk");
    CHECK(Error("  .include b.inc\n") == "t.asm line 3: .include needs \"file\"");
    std::filesystem::remove_all(dir);
}

TEST_CASE("asm: .incbin", "[asm]") {
    const auto dir = std::filesystem::temp_directory_path() / "bw_asm_incbin_test";
    std::filesystem::create_directories(dir);
    const auto write = [&](const std::filesystem::path& path, std::string_view text) {
        std::ofstream(dir / path, std::ios::binary) << text;
    };
    write("data.bin", std::string("\x10\x20\x30\x40\x00\xFF", 6));
    write("a.asm", "  .rom $F000, $1000\n  .org $F000\n  .incbin \"data.bin\", 1, 4\n  nop\n");
    const auto rom = AssembleFile(dir / "a.asm");
    REQUIRE(rom.has_value());
    CHECK(Bytes_(rom->words.begin(), rom->words.begin() + 5) ==
          Bytes_{0x20, 0x30, 0x40, 0x00, 0xEA});

    write("past.asm", "  .rom $F000, $1000\n  .org $F000\n  .incbin \"data.bin\", 4, 3\n");
    CHECK(AssembleFile(dir / "past.asm").error() ==
          "past.asm line 3: .incbin reads past the end of data.bin");
    write("miss.asm", "  .rom $F000, $1000\n  .org $F000\n  .incbin \"none.bin\", 0, 1\n");
    CHECK(AssembleFile(dir / "miss.asm").error() == "miss.asm line 3: cannot read 'none.bin'");
    write("args.asm", "  .rom $F000, $1000\n  .org $F000\n  .incbin \"data.bin\", 0\n");
    CHECK(AssembleFile(dir / "args.asm").error() ==
          "args.asm line 3: .incbin needs \"file\", OFFSET, LENGTH");
    CHECK(Error("  .incbin \"data.bin\", 0, 1\n") == "t.asm line 3: .incbin needs a file on disk");
    std::filesystem::remove_all(dir);
}

TEST_CASE("asm: every ROM program matches its committed .rom", "[asm]") {
    std::vector<std::filesystem::directory_entry> programs;
    for (const auto& machine : std::filesystem::directory_iterator(test::RootDir() / "machines")) {
        for (const char* dir : {"roms", "debug-roms"}) {
            if (std::filesystem::is_directory(machine.path() / dir)) {
                for (const auto& entry :
                     std::filesystem::directory_iterator(machine.path() / dir)) {
                    programs.push_back(entry);
                }
            }
        }
    }
    for (const auto& entry : std::filesystem::directory_iterator(test::RootDir() / "bench")) {
        programs.push_back(entry);
    }
    for (const auto& entry : programs) {
        if (entry.path().extension() != ".asm") {
            continue;
        }
        INFO(entry.path().filename().string());
        const auto rom = AssembleFile(entry.path());
        REQUIRE(rom.has_value());
        std::ifstream in(std::filesystem::path(entry.path()).replace_extension(".rom"),
                         std::ios::binary);
        REQUIRE(in);
        std::ostringstream committed;
        committed << in.rdbuf();
        CHECK(committed.str() == rom->text);
    }
}
