#include <array>
#include <cstdint>
#include <format>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "asm/assembler.hpp"
#include "asm/z80_disassembler.hpp"
#include "parts/z80_core.hpp"
#include "z80_opcodes.hpp"

using namespace bw;

namespace {

constexpr std::string_view kHeader = "  .cpu z80\n  .rom $0000, $1000\n  .org $0100\n";

// The bytes a Z80 program assembles to, read back from the .rom text's @ rows: the header
// emits none, so they are exactly the body's, $FF bytes included.
std::vector<std::uint8_t> Z80Bytes(std::string_view body) {
    const auto rom = Assemble(std::string(kHeader) + std::string(body), "z.asm");
    if (!rom) {
        FAIL(rom.error());
    }
    std::vector<std::uint8_t> out;
    std::istringstream lines(rom->text);
    for (std::string line; std::getline(lines, line);) {
        if (!line.starts_with('@')) {
            continue;
        }
        std::istringstream row(line.substr(0, line.find(';')));
        std::string token;
        row >> token;  // the offset
        while (row >> token) {
            out.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
        }
    }
    return out;
}

std::string Z80Error(std::string_view body) {
    const auto rom = Assemble(std::string(kHeader) + std::string(body), "z.asm");
    REQUIRE_FALSE(rom.has_value());
    return rom.error();
}

std::string Hex(const std::vector<std::uint8_t>& bytes) {
    std::string out;
    for (const std::uint8_t b : bytes) {
        out += std::format("{}{:02X}", out.empty() ? "" : " ", b);
    }
    return out;
}

struct Row {
    std::string_view text;
    std::vector<std::uint8_t> bytes;
};

// One example of each documented form, its bytes as Zilog's UM0080 gives them; assembled at
// $0100 (relative targets from there).
const std::vector<Row>& HandTable() {
    static const std::vector<Row> rows{
        // 8-bit load
        {"LD B,C", {0x41}},
        {"LD A,L", {0x7D}},
        {"LD E,A", {0x5F}},
        {"LD D,$12", {0x16, 0x12}},
        {"LD H,(HL)", {0x66}},
        {"LD C,(IX+5)", {0xDD, 0x4E, 0x05}},
        {"LD L,(IY-3)", {0xFD, 0x6E, 0xFD}},
        {"LD (HL),B", {0x70}},
        {"LD (IX+0),A", {0xDD, 0x77, 0x00}},
        {"LD (IY-128),E", {0xFD, 0x73, 0x80}},
        {"LD (HL),$AA", {0x36, 0xAA}},
        {"LD (IX+2),$55", {0xDD, 0x36, 0x02, 0x55}},
        {"LD (IY-1),$01", {0xFD, 0x36, 0xFF, 0x01}},
        {"LD A,(BC)", {0x0A}},
        {"LD A,(DE)", {0x1A}},
        {"LD A,($4321)", {0x3A, 0x21, 0x43}},
        {"LD (BC),A", {0x02}},
        {"LD (DE),A", {0x12}},
        {"LD ($4321),A", {0x32, 0x21, 0x43}},
        {"LD A,I", {0xED, 0x57}},
        {"LD A,R", {0xED, 0x5F}},
        {"LD I,A", {0xED, 0x47}},
        {"LD R,A", {0xED, 0x4F}},
        // 16-bit load
        {"LD BC,$1234", {0x01, 0x34, 0x12}},
        {"LD SP,$8000", {0x31, 0x00, 0x80}},
        {"LD IX,$1234", {0xDD, 0x21, 0x34, 0x12}},
        {"LD IY,$1234", {0xFD, 0x21, 0x34, 0x12}},
        {"LD HL,($5000)", {0x2A, 0x00, 0x50}},
        {"LD DE,($5000)", {0xED, 0x5B, 0x00, 0x50}},
        {"LD SP,($5000)", {0xED, 0x7B, 0x00, 0x50}},
        {"LD IX,($5000)", {0xDD, 0x2A, 0x00, 0x50}},
        {"LD ($5000),HL", {0x22, 0x00, 0x50}},
        {"LD ($5000),BC", {0xED, 0x43, 0x00, 0x50}},
        {"LD ($5000),IY", {0xFD, 0x22, 0x00, 0x50}},
        {"LD SP,HL", {0xF9}},
        {"LD SP,IX", {0xDD, 0xF9}},
        {"PUSH AF", {0xF5}},
        {"PUSH DE", {0xD5}},
        {"PUSH IY", {0xFD, 0xE5}},
        {"POP HL", {0xE1}},
        {"POP AF", {0xF1}},
        {"POP IX", {0xDD, 0xE1}},
        // exchange, block
        {"EX DE,HL", {0xEB}},
        {"EX AF,AF'", {0x08}},
        {"EXX", {0xD9}},
        {"EX (SP),HL", {0xE3}},
        {"EX (SP),IX", {0xDD, 0xE3}},
        {"LDI", {0xED, 0xA0}},
        {"LDIR", {0xED, 0xB0}},
        {"LDD", {0xED, 0xA8}},
        {"LDDR", {0xED, 0xB8}},
        {"CPI", {0xED, 0xA1}},
        {"CPIR", {0xED, 0xB1}},
        {"CPD", {0xED, 0xA9}},
        {"CPDR", {0xED, 0xB9}},
        // 8-bit arithmetic and logic
        {"ADD A,B", {0x80}},
        {"ADD A,(HL)", {0x86}},
        {"ADD A,(IX+1)", {0xDD, 0x86, 0x01}},
        {"ADD A,$10", {0xC6, 0x10}},
        {"ADC A,C", {0x89}},
        {"ADC A,$10", {0xCE, 0x10}},
        {"SUB D", {0x92}},
        {"SUB $10", {0xD6, 0x10}},
        {"SBC A,E", {0x9B}},
        {"SBC A,(IY+2)", {0xFD, 0x9E, 0x02}},
        {"SBC A,$10", {0xDE, 0x10}},
        {"AND H", {0xA4}},
        {"AND $0F", {0xE6, 0x0F}},
        {"XOR A", {0xAF}},
        {"XOR $FF", {0xEE, 0xFF}},
        {"OR L", {0xB5}},
        {"OR (HL)", {0xB6}},
        {"OR $80", {0xF6, 0x80}},
        {"CP A", {0xBF}},
        {"CP (IX-2)", {0xDD, 0xBE, 0xFE}},
        {"CP $20", {0xFE, 0x20}},
        {"INC B", {0x04}},
        {"INC A", {0x3C}},
        {"INC (HL)", {0x34}},
        {"INC (IX+7)", {0xDD, 0x34, 0x07}},
        {"DEC C", {0x0D}},
        {"DEC (HL)", {0x35}},
        {"DEC (IY+7)", {0xFD, 0x35, 0x07}},
        // general purpose
        {"DAA", {0x27}},
        {"CPL", {0x2F}},
        {"NEG", {0xED, 0x44}},
        {"CCF", {0x3F}},
        {"SCF", {0x37}},
        {"NOP", {0x00}},
        {"HALT", {0x76}},
        {"DI", {0xF3}},
        {"EI", {0xFB}},
        {"IM 0", {0xED, 0x46}},
        {"IM 1", {0xED, 0x56}},
        {"IM 2", {0xED, 0x5E}},
        // 16-bit arithmetic
        {"ADD HL,BC", {0x09}},
        {"ADD HL,HL", {0x29}},
        {"ADD HL,SP", {0x39}},
        {"ADC HL,DE", {0xED, 0x5A}},
        {"SBC HL,BC", {0xED, 0x42}},
        {"SBC HL,SP", {0xED, 0x72}},
        {"ADD IX,BC", {0xDD, 0x09}},
        {"ADD IX,IX", {0xDD, 0x29}},
        {"ADD IY,IY", {0xFD, 0x29}},
        {"ADD IY,SP", {0xFD, 0x39}},
        {"INC DE", {0x13}},
        {"INC SP", {0x33}},
        {"INC IX", {0xDD, 0x23}},
        {"DEC BC", {0x0B}},
        {"DEC HL", {0x2B}},
        {"DEC IY", {0xFD, 0x2B}},
        // rotate and shift
        {"RLCA", {0x07}},
        {"RLA", {0x17}},
        {"RRCA", {0x0F}},
        {"RRA", {0x1F}},
        {"RLC B", {0xCB, 0x00}},
        {"RLC (HL)", {0xCB, 0x06}},
        {"RLC (IX+3)", {0xDD, 0xCB, 0x03, 0x06}},
        {"RRC A", {0xCB, 0x0F}},
        {"RL C", {0xCB, 0x11}},
        {"RR (IY-1)", {0xFD, 0xCB, 0xFF, 0x1E}},
        {"SLA E", {0xCB, 0x23}},
        {"SRA (HL)", {0xCB, 0x2E}},
        {"SRL A", {0xCB, 0x3F}},
        {"RLD", {0xED, 0x6F}},
        {"RRD", {0xED, 0x67}},
        // bit
        {"BIT 0,B", {0xCB, 0x40}},
        {"BIT 7,A", {0xCB, 0x7F}},
        {"BIT 3,(HL)", {0xCB, 0x5E}},
        {"BIT 6,(IX+4)", {0xDD, 0xCB, 0x04, 0x76}},
        {"SET 1,C", {0xCB, 0xC9}},
        {"SET 7,(IY+0)", {0xFD, 0xCB, 0x00, 0xFE}},
        {"RES 2,D", {0xCB, 0x92}},
        {"RES 0,(HL)", {0xCB, 0x86}},
        // jump
        {"JP $1234", {0xC3, 0x34, 0x12}},
        {"JP NZ,$1234", {0xC2, 0x34, 0x12}},
        {"JP Z,$1234", {0xCA, 0x34, 0x12}},
        {"JP NC,$1234", {0xD2, 0x34, 0x12}},
        {"JP C,$1234", {0xDA, 0x34, 0x12}},
        {"JP PO,$1234", {0xE2, 0x34, 0x12}},
        {"JP PE,$1234", {0xEA, 0x34, 0x12}},
        {"JP P,$1234", {0xF2, 0x34, 0x12}},
        {"JP M,$1234", {0xFA, 0x34, 0x12}},
        {"JR $0110", {0x18, 0x0E}},
        {"JR $0100", {0x18, 0xFE}},
        {"JR NZ,$0102", {0x20, 0x00}},
        {"JR Z,$0180", {0x28, 0x7E}},
        {"JR NC,$0082", {0x30, 0x80}},
        {"JR C,$0101", {0x38, 0xFF}},
        {"JP (HL)", {0xE9}},
        {"JP (IX)", {0xDD, 0xE9}},
        {"JP (IY)", {0xFD, 0xE9}},
        {"DJNZ $00F0", {0x10, 0xEE}},
        // call and return
        {"CALL $1234", {0xCD, 0x34, 0x12}},
        {"CALL NZ,$1234", {0xC4, 0x34, 0x12}},
        {"CALL C,$1234", {0xDC, 0x34, 0x12}},
        {"CALL M,$1234", {0xFC, 0x34, 0x12}},
        {"RET", {0xC9}},
        {"RET Z", {0xC8}},
        {"RET NC", {0xD0}},
        {"RET PE", {0xE8}},
        {"RETI", {0xED, 0x4D}},
        {"RETN", {0xED, 0x45}},
        {"RST $00", {0xC7}},
        {"RST $08", {0xCF}},
        {"RST $38", {0xFF}},
        // input and output
        {"IN A,($FE)", {0xDB, 0xFE}},
        {"IN B,(C)", {0xED, 0x40}},
        {"IN A,(C)", {0xED, 0x78}},
        {"IN L,(C)", {0xED, 0x68}},
        {"INI", {0xED, 0xA2}},
        {"INIR", {0xED, 0xB2}},
        {"IND", {0xED, 0xAA}},
        {"INDR", {0xED, 0xBA}},
        {"OUT ($FE),A", {0xD3, 0xFE}},
        {"OUT (C),D", {0xED, 0x51}},
        {"OUT (C),A", {0xED, 0x79}},
        {"OUTI", {0xED, 0xA3}},
        {"OTIR", {0xED, 0xB3}},
        {"OUTD", {0xED, 0xAB}},
        {"OTDR", {0xED, 0xBB}},
    };
    return rows;
}

// Runs `bytes` at $0100 on the core until the instruction ends; the Fetch and Read addresses in
// $0100-$010F, in order.
std::vector<std::uint16_t> OwnReads(const std::vector<std::uint8_t>& bytes) {
    static std::array<std::uint8_t, 0x10000> ram{};
    ram.fill(0);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        ram[0x0100 + i] = bytes[i];
    }
    Z80Core core;
    core.start(
        {.pc = 0x0100, .sp = 0x9000, .ix = 0x8000, .iy = 0x8000, .b = 0x80, .d = 0x80, .h = 0x80});
    std::vector<std::uint16_t> reads;
    for (int cycles = 0; cycles < 64; ++cycles) {
        const Z80Cycle bus = core.cycle();
        std::uint8_t data = 0xFF;
        if (bus.kind == Z80CycleKind::Fetch || bus.kind == Z80CycleKind::Read) {
            data = ram[bus.address];
            if (bus.address >= 0x0100 && bus.address < 0x0110) {
                reads.push_back(bus.address);
            }
        } else if (bus.kind == Z80CycleKind::Write) {
            ram[bus.address] = bus.data;
        }
        core.step(data);
        if (core.instructionEnded()) {
            break;
        }
    }
    return reads;
}

}  // namespace

TEST_CASE("asm z80: every documented form, bytes from Zilog's manual", "[asm]") {
    for (const Row& row : HandTable()) {
        INFO(row.text);
        CHECK(Hex(Z80Bytes(std::format("  {}\n", row.text))) == Hex(row.bytes));
    }
}

TEST_CASE("asm z80: lower case, blanks and the hand table on the disassembler", "[asm]") {
    CHECK(Hex(Z80Bytes("  ld  b , ( ix - 3 )\n")) == "DD 46 FD");
    CHECK(Hex(Z80Bytes("  Ex Af,aF'\n")) == "08");
    // The disassembler reads each hand row back as the manual writes it.
    for (const Row& row : HandTable()) {
        INFO(row.text);
        const auto text = DisassembleZ80(row.bytes, 0x0100);
        REQUIRE(text.has_value());
        CHECK(text->length == row.bytes.size());
        CHECK(Hex(Z80Bytes("  " + text->text + "\n")) == Hex(row.bytes));
    }
}

TEST_CASE("asm z80: every documented opcode round-trips through the disassembler", "[asm]") {
    const auto opcodes = Z80DocumentedOpcodes();
    CHECK(opcodes.size() == 696);
    std::set<std::string> texts;
    for (const auto& bytes : opcodes) {
        const auto text = DisassembleZ80(bytes, 0x0100);
        REQUIRE(text.has_value());
        INFO(Hex(bytes) + "  " + text->text);
        CHECK(Hex(Z80Bytes("  " + text->text + "\n")) == Hex(bytes));
        texts.insert(text->text);
    }
    CHECK(texts.size() == opcodes.size());  // no two opcodes read the same
}

TEST_CASE("asm z80: every documented opcode takes exactly its bytes on the core", "[asm]") {
    for (const auto& bytes : Z80DocumentedOpcodes()) {
        INFO(Hex(bytes));
        std::vector<std::uint16_t> expected;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            expected.push_back(static_cast<std::uint16_t>(0x0100 + i));
        }
        CHECK(OwnReads(bytes) == expected);
    }
}

TEST_CASE("asm z80: parentheses, displacements and relative jumps", "[asm]") {
    CHECK(Hex(Z80Bytes("BUF = $4000\n  LD A,(BUF+1)\n")) == "3A 01 40");
    CHECK(Hex(Z80Bytes("  LD A,(1+2)*3\n")) == "3E 09");
    CHECK(Hex(Z80Bytes("  LD A,(2)+(3)\n")) == "3E 05");
    CHECK(Hex(Z80Bytes("  LD A,(IX)\n")) == "DD 7E 00");
    CHECK(Hex(Z80Bytes("  LD A,(IX-1)\n")) == "DD 7E FF");
    CHECK(Hex(Z80Bytes("  LD A,(IY+127)\n")) == "FD 7E 7F");
    CHECK(Hex(Z80Bytes("D = 4\n  LD A,(IY+D*2)\n")) == "FD 7E 08");
    CHECK(Hex(Z80Bytes("IXVAL = $1234\n  LD HL,(IXVAL)\n")) == "2A 34 12");
    CHECK(Hex(Z80Bytes("  JR *\n")) == "18 FE");
    CHECK(Hex(Z80Bytes("  JR later\n  JP later\nlater: NOP\n")) == "18 03 C3 05 01 00");
    CHECK(Hex(Z80Bytes("loop: DJNZ loop\n")) == "10 FE");
    CHECK(Hex(Z80Bytes("  EX AF,AF' ; it's\n")) == "08");
    CHECK(Hex(Z80Bytes("  CP 'A'\n  LD A,','\n")) == "FE 41 3E 2C");
    CHECK(Hex(Z80Bytes("  LD HL,-1\n  LD A,-1\n")) == "21 FF FF 3E FF");
    CHECK(Hex(Z80Bytes("Loop: NOP\n  JP Loop\n")) == "00 C3 00 01");  // symbols keep their case
}

TEST_CASE("asm z80: a symbol named like a register reads as the register", "[asm]") {
    CHECK(Hex(Z80Bytes("C = 5\n  LD A,C\n  LD A,C+1\n")) == "79 3E 06");
    CHECK(Hex(Z80Bytes("Z = 5\n  LD A,Z\n")) == "3E 05");  // Z is a condition only in JP etc.
    CHECK(Z80Error("  JP C\n") == "z.asm line 4: JP has no form 'C'");
}

TEST_CASE("asm z80: errors", "[asm]") {
    CHECK(Z80Error("  LD A,256\n") == "z.asm line 4: immediate value 256 out of range");
    CHECK(Z80Error("  LD A,(IX+128)\n") == "z.asm line 4: displacement value 128 out of range");
    CHECK(Z80Error("  BIT 8,A\n") == "z.asm line 4: bit value 8 out of range");
    CHECK(Z80Error("  IM 3\n") == "z.asm line 4: interrupt mode value 3 out of range");
    CHECK(Z80Error("  RST 1\n") == "z.asm line 4: RST needs $00, $08, ... $38");
    CHECK(Z80Error("  IN A,($100)\n") == "z.asm line 4: port value 256 out of range");
    CHECK(Z80Error("  JR $0200\n") == "z.asm line 4: relative jump out of range (254 bytes)");
    CHECK(Z80Error("  ADD B\n") == "z.asm line 4: ADD has no form 'B' (Zilog: ADD A,s)");
    CHECK(Z80Error("  SUB A,B\n") == "z.asm line 4: SUB has no form 'A,B' (Zilog: SUB s)");
    CHECK(Z80Error("  LD B,(SP)\n") == "z.asm line 4: LD has no form 'B,(SP)'");
    CHECK(Z80Error("  LD (HL),(HL)\n") == "z.asm line 4: LD has no form '(HL),(HL)'");
    CHECK(Z80Error("  LD (IX+1),(HL)\n") == "z.asm line 4: LD has no form '(IX+1),(HL)'");
    CHECK(Z80Error("  ADD IX,HL\n") == "z.asm line 4: ADD has no form 'IX,HL'");
    CHECK(Z80Error("  ADD IX,IY\n") == "z.asm line 4: ADD has no form 'IX,IY'");
    CHECK(Z80Error("  JR PO,$0100\n") == "z.asm line 4: JR has no form 'PO,$0100'");
    CHECK(Z80Error("  LD A,IXH\n") == "z.asm line 4: undefined symbol 'IXH'");
    CHECK(Z80Error("  SLL B\n") == "z.asm line 4: unknown instruction 'SLL'");
    CHECK(Z80Error("  lda #1\n") == "z.asm line 4: unknown instruction 'lda'");
    CHECK(Z80Error("  JP undefined\n") == "z.asm line 4: undefined symbol 'undefined'");
    const auto noOrg = Assemble("  .cpu z80\n  .rom 0, 16\n  NOP\n", "n.asm");
    REQUIRE_FALSE(noOrg.has_value());
    CHECK(noOrg.error() == "n.asm line 3: instruction before any .org");
    const auto noCpu = Assemble("  .rom 0, 16\n  .org 0\n  LD A,B\n", "n.asm");
    REQUIRE_FALSE(noCpu.has_value());
    CHECK(noCpu.error() == "n.asm line 3: unknown instruction 'LD'");
}

TEST_CASE("asm z80: directives as on the 6502", "[asm]") {
    CHECK(Hex(Z80Bytes("  .byte 1, \"AB\"\n  .word $1234\n")) == "01 41 42 34 12");
    const auto rom =
        Assemble("  .cpu z80\n  .rom $0000, $100\n  .org 0\n  LD A,$12\n  .checksum\n", "c.asm");
    REQUIRE(rom.has_value());
    unsigned sum = 0;
    for (const std::uint8_t w : rom->words) {
        sum += w;
    }
    CHECK(sum % 256 == 0);
}
