#include "parts/mos6502_core.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

constexpr std::uint16_t kNmi = 0x3000, kReset = 0x0200, kIrq = 0x4000;

// A core on 64 KiB of NOPs with the three vectors set; every cycle kept.
struct System {
    std::array<std::uint8_t, 0x10000> ram{};
    Mos6502Core core;
    std::vector<Mos6502Cycle> trace;

    explicit System(std::uint8_t p, std::uint16_t pc = kReset, std::uint8_t s = 0xFD) {
        ram.fill(0xEA);
        for (const auto [vector, target] :
             {std::pair{0xFFFA, kNmi}, std::pair{0xFFFC, kReset}, std::pair{0xFFFE, kIrq}}) {
            ram[static_cast<std::size_t>(vector)] = static_cast<std::uint8_t>(target & 0xFF);
            ram[static_cast<std::size_t>(vector + 1)] = static_cast<std::uint8_t>(target >> 8);
        }
        core.start({.pc = pc, .a = 0, .x = 0, .y = 0, .s = s, .p = p});
    }
    void load(std::uint16_t at, std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            ram[at++] = b;
        }
    }
    void cycle() {
        const Mos6502Cycle bus = core.cycle();
        trace.push_back(bus);
        if (bus.read) {
            core.step(ram[bus.address]);
        } else {
            ram[bus.address] = bus.data;
            core.step(0);
        }
    }
    void cycles(int n) {
        for (int i = 0; i < n; ++i) {
            cycle();
        }
    }
    // Cycles until the next opcode fetch is in progress.
    void toFetch() {
        do {
            cycle();
        } while (!core.cycle().sync);
    }
    int countReads(std::uint16_t address) const {
        int n = 0;
        for (const auto& bus : trace) {
            n += bus.read && bus.address == address ? 1 : 0;
        }
        return n;
    }
};

constexpr std::uint8_t kI = 0x04, kB = 0x10, kV = 0x40;

}  // namespace

TEST_CASE("6502 core: NOP takes two cycles", "[parts]") {
    System sys(0x24, 0x1234);
    CHECK(sys.core.cycle().address == 0x1234);
    CHECK(sys.core.cycle().sync);
    sys.cycle();
    CHECK(sys.core.cycle().address == 0x1235);
    CHECK_FALSE(sys.core.cycle().sync);
    CHECK(sys.core.cycle().read);
    sys.cycle();
    CHECK(sys.core.cycle().address == 0x1235);
    CHECK(sys.core.cycle().sync);
}

TEST_CASE("6502 core: reset", "[parts]") {
    System sys(0x00, 0x1234, 0x80);
    sys.load(0x1234, {0x20, 0x00, 0x50});  // JSR $5000, abandoned in its third cycle
    sys.cycles(3);
    sys.core.setReset(true);
    sys.cycle();
    const std::uint16_t pc = sys.core.registers().pc;
    for (int i = 0; i < 3; ++i) {  // held: reads at PC
        CHECK(sys.core.cycle().address == pc);
        CHECK(sys.core.cycle().read);
        CHECK_FALSE(sys.core.cycle().sync);
        sys.cycle();
    }
    sys.core.setReset(false);
    sys.cycle();  // seen released: that cycle was T1
    const std::uint8_t s = sys.core.registers().s;
    const std::array<std::uint16_t, 6> expected{
        pc,
        static_cast<std::uint16_t>(0x0100 | s),
        static_cast<std::uint16_t>(0x0100 | static_cast<std::uint8_t>(s - 1)),
        static_cast<std::uint16_t>(0x0100 | static_cast<std::uint8_t>(s - 2)),
        0xFFFC,
        0xFFFD};
    for (const std::uint16_t address : expected) {
        CHECK(sys.core.cycle().address == address);
        CHECK(sys.core.cycle().read);
        sys.cycle();
    }
    CHECK(sys.core.cycle().address == kReset);
    CHECK(sys.core.cycle().sync);
    CHECK(sys.core.registers().s == static_cast<std::uint8_t>(s - 3));
    CHECK((sys.core.registers().p & kI) != 0);
}

TEST_CASE("6502 core: IRQ", "[parts]") {
    SECTION("taken after the instruction that follows CLI, B clear, vector FFFE") {
        System sys(kI);
        sys.load(kReset, {0x58});  // CLI, then NOPs
        sys.core.setIrq(true);
        sys.toFetch();  // CLI done
        CHECK(sys.core.cycle().address == kReset + 1);
        sys.toFetch();  // the NOP still runs under the old I
        CHECK(sys.core.cycle().address == kReset + 2);
        sys.toFetch();  // the interrupt sequence
        CHECK(sys.core.cycle().address == kIrq);
        CHECK(sys.ram[0x01FD] == 0x02);
        CHECK(sys.ram[0x01FC] == 0x02);  // PC of the instruction not run
        CHECK((sys.ram[0x01FB] & kB) == 0);
        CHECK(sys.countReads(0xFFFE) == 1);
        CHECK((sys.core.registers().p & kI) != 0);
    }
    SECTION("masked by I") {
        System sys(kI);
        sys.core.setIrq(true);
        sys.cycles(40);
        CHECK(sys.countReads(0xFFFE) == 0);
        CHECK(sys.core.registers().s == 0xFD);
    }
    SECTION("asserted only in an instruction's last cycle waits one more instruction") {
        System sys(0x00);
        sys.cycle();  // NOP T1
        sys.core.setIrq(true);
        sys.cycle();  // NOP T2, the last
        sys.toFetch();
        CHECK(sys.core.cycle().address == kReset + 2);  // the next NOP ran
        sys.toFetch();
        CHECK(sys.core.cycle().address == kIrq);
        CHECK(sys.ram[0x01FC] == 0x02);
    }
    SECTION("a taken branch in its page delays it by one instruction") {
        System sys(0x00);
        sys.load(kReset, {0x90, 0x00});  // BCC +0, taken, same page
        sys.cycle();                     // T1
        sys.core.setIrq(true);           // seen at the end of T2, the second-to-last cycle
        sys.cycle();                     // T2
        sys.cycle();                     // T3, the last
        CHECK(sys.core.cycle().address == kReset + 2);
        sys.toFetch();  // the NOP after the branch runs
        CHECK(sys.core.cycle().address == kReset + 3);
        sys.toFetch();
        CHECK(sys.core.cycle().address == kIrq);
    }
}

TEST_CASE("6502 core: NMI", "[parts]") {
    SECTION("once per falling edge, whatever I") {
        System sys(kI);
        sys.core.setNmi(true);
        sys.cycles(60);
        CHECK(sys.countReads(0xFFFA) == 1);
        sys.core.setNmi(false);
        sys.cycles(4);
        sys.core.setNmi(true);
        sys.cycles(40);
        CHECK(sys.countReads(0xFFFA) == 2);
    }
    SECTION("takes over a BRK before its vector is chosen") {
        System sys(0x00);
        sys.load(kReset, {0x00, 0x00});  // BRK
        sys.cycles(3);                   // T1-T3
        sys.core.setNmi(true);
        sys.cycle();  // T4 ends: the vector is chosen
        sys.toFetch();
        CHECK(sys.core.cycle().address == kNmi);
        CHECK(sys.countReads(0xFFFE) == 0);
        CHECK((sys.ram[0x01FB] & kB) != 0);  // pushed by a BRK
    }
}

TEST_CASE("6502 core: JAM", "[parts]") {
    System sys(0x00);
    sys.load(kReset, {0x02});
    sys.core.setIrq(true);
    sys.core.setNmi(true);
    sys.cycles(40);
    const std::array<std::uint16_t, 8> start{kReset, kReset + 1, 0xFFFF, 0xFFFE,
                                             0xFFFE, 0xFFFF,     0xFFFF, 0xFFFF};
    for (std::size_t i = 0; i < start.size(); ++i) {
        CHECK(sys.trace[i].address == start[i]);
    }
    CHECK(sys.trace.back().address == 0xFFFF);
    CHECK(sys.core.jammed());
    for (const auto& bus : sys.trace) {
        CHECK(bus.read);
    }
    sys.core.setReset(true);
    sys.cycles(2);
    sys.core.setReset(false);
    sys.toFetch();
    CHECK_FALSE(sys.core.jammed());
    CHECK(sys.core.cycle().address == kReset);
}

TEST_CASE("6502 core: SO sets V", "[parts]") {
    System sys(0x00);
    sys.cycles(3);
    sys.core.setOverflow();
    CHECK((sys.core.registers().p & kV) != 0);
}

TEST_CASE("6502: decode gives the 151 documented opcodes", "[parts]") {
    int documented = 0;
    for (int code = 0; code < 256; ++code) {
        documented += Mos6502Decode(static_cast<std::uint8_t>(code)).documented ? 1 : 0;
    }
    CHECK(documented == 151);

    const auto is = [](std::uint8_t code, std::string_view mnemonic, Mos6502Operand operand,
                       bool doc) {
        const Mos6502Instruction i = Mos6502Decode(code);
        CHECK(i.mnemonic == mnemonic);
        CHECK(i.operand == operand);
        CHECK(i.documented == doc);
    };
    using enum Mos6502Operand;
    is(0xA9, "LDA", Immediate, true);
    is(0x6C, "JMP", Indirect, true);
    is(0x4C, "JMP", Absolute, true);
    is(0x20, "JSR", Absolute, true);
    is(0x96, "STX", ZeroPageY, true);
    is(0xB1, "LDA", IndirectY, true);
    is(0xA1, "LDA", IndirectX, true);
    is(0x0A, "ASL", Accumulator, true);
    is(0xD0, "BNE", Relative, true);
    is(0x60, "RTS", None, true);
    is(0x00, "BRK", None, true);
    is(0xEA, "NOP", None, true);
    is(0xEB, "SBC", Immediate, false);
    is(0x1A, "NOP", None, false);
    is(0xA7, "LAX", ZeroPage, false);
    is(0x02, "JAM", None, false);
}
