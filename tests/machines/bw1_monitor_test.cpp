// The BW-1 monitor (machines/120-bw1/roms/bw1_002.asm, docs/p5k-bw1-monitor/BW1_MONITOR.md) on the
// KIM model, used from its keypad as a player would.

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../paths.hpp"
#include "bw1_model.hpp"
#include "parts/rom_contents.hpp"

using namespace bw;

namespace {

constexpr std::uint16_t kAcc = 0xF3, kSpUser = 0xF2, kMode = 0xFF, kPcl = 0xEF, kPch = 0xF0;

// What the digits show: the address and the byte there.
std::string Shown(const Bw1Model& model, std::uint16_t address) {
    return std::format("{:04X}{:02X}", address, model.peek(address));
}

std::uint16_t Word(const Bw1Model& model, std::uint16_t address) {
    return static_cast<std::uint16_t>(model.peek(address) | model.peek(address + 1) << 8);
}

}  // namespace

TEST_CASE("bw1 monitor: reset shows 0200 and sets the vectors", "[machines]") {
    const auto seed = GENERATE(1u, 2u, 3u);
    Bw1Model model(seed);
    model.run(20'000);
    CHECK(model.display() == Shown(model, 0x0200));
    CHECK(model.peek(kMode) == 1);
    CHECK(Word(model, 0x17FA) == 0x1C00);
    CHECK(Word(model, 0x17FE) == 0x1C00);
    CHECK(model.peek(kSpUser) == 0xFF);
    CHECK_FALSE(model.timerTouched());
}

TEST_CASE("bw1 monitor: AD, DA and + ", "[machines]") {
    Bw1Model model(1);
    model.run(20'000);
    model.type("AD 0300 DA A9");
    CHECK(model.peek(0x0300) == 0xA9);
    CHECK(model.display() == "0300A9");
    model.type("+");
    CHECK(model.display() == Shown(model, 0x0301));
    model.type("+ 5");  // still data mode: into $0302
    CHECK(model.peek(0x0302) % 16 == 5);
    model.type("AD 0123");
    CHECK(model.display() == Shown(model, 0x0123));
    model.type("AD 12345");  // the last four
    CHECK(model.display() == Shown(model, 0x2345));
    CHECK_FALSE(model.timerTouched());
}

TEST_CASE("bw1 monitor: a program typed in, run by GO, stopped by BRK", "[machines]") {
    const auto seed = GENERATE(1u, 2u, 3u);
    Bw1Model model(seed);
    model.run(20'000);
    model.type("AD 0200 DA A9 + 42 + 85 + 10 + 00");  // LDA #$42, STA $10, BRK
    model.type("AD 0200 GO");
    CHECK(model.peek(0x10) == 0x42);
    CHECK(model.display() == Shown(model, 0x0206));  // the BRK's address + 2
    CHECK(Word(model, kPcl) == 0x0206);
    CHECK(model.peek(kPch) == 0x02);
    model.type("AD 0300");
    CHECK(model.display() == Shown(model, 0x0300));
    model.type("PC");
    CHECK(model.display() == Shown(model, 0x0206));
    CHECK_FALSE(model.timerTouched());
}

TEST_CASE("bw1 monitor: GO uses the saved registers", "[machines]") {
    Bw1Model model(2);
    model.run(20'000);
    model.type("AD 00F3 DA 5A");                    // ACC
    model.type("AD 0200 DA 85 + 10 + 00 AD 0200");  // STA $10, BRK
    model.type("GO");
    CHECK(model.peek(0x10) == 0x5A);
    CHECK(model.peek(kAcc) == 0x5A);
}

TEST_CASE("bw1 monitor: ST stops a running program", "[machines]") {
    Bw1Model model(3);
    model.run(20'000);
    model.type("AD 0200 DA 4C + 00 + 02 AD 0200 GO");  // JMP $0200
    model.run(50'000);
    model.stop();
    model.run(20'000);
    CHECK(model.display() == "02004C");
}

TEST_CASE("bw1 monitor: SST runs one instruction a GO", "[machines]") {
    Bw1Model model(1);
    model.run(20'000);
    model.type("AD 0200 DA A9 + 11 + A9 + 22 + A9 + 33 AD 0200");  // LDA #$11, #$22, #$33
    model.setSst(true);
    model.type("GO");
    CHECK(model.display() == "0202A9");
    CHECK(model.peek(kAcc) == 0x11);
    model.type("GO");
    CHECK(model.display() == "0204A9");
    CHECK(model.peek(kAcc) == 0x22);
    model.type("GO");
    CHECK(model.display() == Shown(model, 0x0206));
    CHECK(model.peek(kAcc) == 0x33);
}

TEST_CASE("bw1 monitor: SCANDS and GETKEY at the KIM's addresses", "[machines]") {
    Bw1Model model(2);
    model.run(20'000);
    // $0200 JSR SCANDS, BNE $0200 (no key: GO is still held), JSR SCANDS, BEQ $0205 (a key),
    // JSR GETKEY, STA $00, BRK.
    model.type("AD 0200 DA 20 + 1F + 1F + D0 + FB + 20 + 1F + 1F + F0 + FB + 20 + 6A + 1F "
               "+ 85 + 00 + 00 AD 0200 GO");
    model.run(50'000);
    model.press(7);
    CHECK(model.peek(0x00) == 7);
    CHECK(model.display() == Shown(model, 0x0211));
}

TEST_CASE("bw1 monitor: a key held is read once", "[machines]") {
    Bw1Model model(1);
    model.run(20'000);
    model.type("AD 0300 DA");
    const std::uint8_t before = model.peek(0x0300);
    model.hold(5);
    model.run(300'000);
    model.release();
    model.run(40'000);
    CHECK(model.peek(0x0300) == static_cast<std::uint8_t>(before << 4 | 5));
}

TEST_CASE("bw1 monitor: the entry points are at the KIM's addresses", "[machines]") {
    const auto rom = ReadRomContents(test::RomPath("bw1", "bw1_002.rom"), 1024, 8);
    REQUIRE(rom.has_value());
    const auto at = [&](std::uint16_t address) { return (*rom)[address - 0x1C00]; };
    CHECK(at(0x1C00) == 0x85);  // SAVE: sta ACC
    CHECK(at(0x1C05) == 0x68);  // SAVE1: pla
    CHECK(at(0x1C0F) == 0x84);  // SAVE2: sty YREG
    CHECK(at(0x1C1C) == 0x6C);  // NMIT: jmp (NMIV)
    CHECK(at(0x1C1F) == 0x6C);  // IRQT: jmp (IRQV)
    CHECK(at(0x1C22) == 0xA9);  // RST
    CHECK(at(0x1C4F) == 0x20);  // START: jsr
    CHECK(at(0x1EFE) == 0x4C);  // AK
    CHECK(at(0x1F19) == 0xA0);  // SCAND: ldy #0
    CHECK(at(0x1F1F) == 0x4C);  // SCANDS
    CHECK(at(0x1F40) == 0xA9);  // KEYIN
    CHECK(at(0x1F48) == 0x4C);  // CONVD
    CHECK(at(0x1F63) == 0xE6);  // INCPT: inc POINTL
    CHECK(at(0x1F6A) == 0x4C);  // GETKEY
    CHECK(at(0x1FE7) == 0xBF);  // TABLE: 0
    CHECK(at(0x1FFB) * 256 + at(0x1FFA) == 0x1C1C);
    CHECK(at(0x1FFD) * 256 + at(0x1FFC) == 0x1C22);
    CHECK(at(0x1FFF) * 256 + at(0x1FFE) == 0x1C1F);
}

TEST_CASE("bw1 monitor: bad RAM in the stack or zero page stops it", "[machines]") {
    const auto seed = GENERATE(1u, 2u, 3u);
    {
        // The reset's first JSR returns through $01FF: stuck at $00, it runs random RAM.
        Bw1Model model(seed);
        for (unsigned bit = 0; bit < 8; ++bit) {
            model.stick(0x01FF, bit, false);
        }
        model.reset();
        model.run(200'000);
        CHECK(model.display().substr(0, 4) != "0200");
    }
    {
        Bw1Model model(seed);
        model.stick(0xFB, 1, false);  // POINTH bit 1
        model.reset();
        model.run(20'000);
        CHECK(model.display() == Shown(model, 0x0000));
    }
}
