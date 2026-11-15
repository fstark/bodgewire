// The KIM model the BW-1's ROMs are tested on (tests/machines/bw1_model.hpp), on small programs
// assembled here and put over the 002's ROM.

#include "bw1_model.hpp"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "asm/assembler.hpp"

using namespace bw;

namespace {

// `body` at $1C00 with the three vectors: RESET $1C00, NMI and IRQ $1F00 (`inc $20`, `rti`).
void Load(Bw1Model& model, const std::string& body) {
    const auto rom = Assemble("  .rom $1C00, $400\n  .org $1C00\n" + body +
                                  "\n  .org $1F00\nnmi: inc $20\n  rti\n"
                                  "  .org $1FFA\n  .word nmi, $1C00, nmi\n",
                              "model.asm");
    REQUIRE(rom.has_value());
    model.patchRom(0x1C00, rom->words);
    model.reset();
}

constexpr const char* kPorts = "  lda #$1E\n  sta $1743\n";  // PB1-PB4 out: U24's inputs

}  // namespace

TEST_CASE("bw1 model: a digit lit by SBD and SAD", "[machines]") {
    Bw1Model model(1);
    Load(model, std::string(kPorts) +
                    "  lda #$7F\n  sta $1741\n  lda #$06\n  sta $1740\n  lda #$0C\n  sta $1742\n"
                    "  jmp *\n");
    CHECK(model.display() == "  1   ");
}

TEST_CASE("bw1 model: a key pulls its column low while its row is selected", "[machines]") {
    Bw1Model model(1);
    // PA all inputs; the row in $10 selected, port A copied to $00.
    Load(model, std::string(kPorts) +
                    "  lda #0\n  sta $1741\nloop: lda $10\n  sta $1742\n  lda $1740\n  sta $00\n"
                    "  jmp loop\n");
    model.poke(0x10, 0x02);  // row 1: 7 8 9 A B C D on PA6-PA0
    model.run(100);
    CHECK(model.peek(0x00) == 0xFF);
    model.hold(9);  // row 1, PA4
    model.run(100);
    CHECK(model.peek(0x00) == 0xEF);
    model.hold(2);  // row 0
    model.run(100);
    CHECK(model.peek(0x00) == 0xFF);
    model.release();
    model.setStrap(true);  // only on row 3, PA0
    model.run(100);
    CHECK(model.peek(0x00) == 0xFF);
    model.poke(0x10, 0x06);
    model.run(100);
    CHECK(model.peek(0x00) == 0xFE);
    model.hold(kKeyPc);  // row 2, PA0: not on row 3
    model.setStrap(false);
    model.run(100);
    CHECK(model.peek(0x00) == 0xFF);
    CHECK_FALSE(model.timerTouched());
}

TEST_CASE("bw1 model: SST stops after instructions outside $1C00-$1FFF", "[machines]") {
    Bw1Model model(1);
    model.poke(0x0200, 0xEA);  // nop, nop, jmp $0200
    model.poke(0x0201, 0xEA);
    model.poke(0x0202, 0x4C);
    model.poke(0x0203, 0x00);
    model.poke(0x0204, 0x02);
    Load(model, "  ldx #$FF\n  txs\n  lda #0\n  sta $20\n  jmp $0200\n");
    model.run(1000);
    CHECK(model.peek(0x20) == 0);
    model.setSst(true);
    model.run(1000);
    CHECK(model.peek(0x20) > 20);  // one NMI an instruction, the handler not stepped
    Load(model, "  ldx #$FF\n  txs\n  lda #0\n  sta $20\nloop: jmp loop\n");
    model.run(1000);
    CHECK(model.peek(0x20) == 0);
    model.stop();  // ST
    CHECK(model.peek(0x20) == 1);
}

TEST_CASE("bw1 model: stuck bits, bus charge and the timer flag", "[machines]") {
    Bw1Model model(2);
    model.stick(0x0234, 3, true);
    model.poke(0x0234, 0);
    CHECK(model.peek(0x0234) == 0x08);
    model.stick(0x17D5, 0, false);
    model.poke(0x17D5, 0xFF);
    CHECK(model.peek(0x17D5) == 0xFE);
    CHECK(model.peek(0x1795) == model.peek(0x1795 | 0xE000));  // A13-A15 not decoded
    // Nothing at $0BCD: the read gets the bus's last byte, the operand's high byte.
    Load(model, "  lda $0BCD\n  sta $00\n  lda #1\n  sta $1744\n  jmp *\n");
    model.run(100);
    CHECK(model.peek(0x00) == 0x0B);
    CHECK(model.timerTouched());
}
