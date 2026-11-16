#include "parts/z80_core.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

// A core on 64 KiB of NOPs; every cycle kept. An In reads `port`; an Acknowledge reads `vector`.
struct System {
    std::array<std::uint8_t, 0x10000> ram{};
    Z80Core core;
    std::vector<Z80Bus> trace;
    std::uint8_t port = 0xFF, vector = 0xFF;
    long tStates = 0;

    explicit System(Z80Registers r = {.pc = 0x1000, .sp = 0x8000}) { core.start(r); }
    void load(std::uint16_t at, std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            ram[at++] = b;
        }
    }
    void cycle() {
        const Z80Bus bus = core.bus();
        trace.push_back(bus);
        tStates += Z80CycleLength(bus.kind, bus.length);
        std::uint8_t data = 0xFF;
        switch (bus.kind) {
        case Z80CycleKind::Fetch:
        case Z80CycleKind::Read:
            data = ram[bus.address];
            break;
        case Z80CycleKind::Write:
            ram[bus.address] = bus.data;
            break;
        case Z80CycleKind::In:
            data = port;
            break;
        case Z80CycleKind::Acknowledge:
            data = vector;
            break;
        default:
            break;
        }
        core.step(data);
    }
    // Cycles until an instruction (or interrupt sequence) ends; its T-states.
    long instruction() {
        const long start = tStates;
        do {
            cycle();
        } while (!core.instructionEnded());
        return tStates - start;
    }
    Z80Registers r() const { return core.registers(); }
    int count(Z80CycleKind kind) const {
        int n = 0;
        for (const auto& bus : trace) {
            n += bus.kind == kind ? 1 : 0;
        }
        return n;
    }
};

}  // namespace

TEST_CASE("Z80 core: NOP and R", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .i = 0x3C, .r = 0xFE});
    CHECK(s.instruction() == 4);
    REQUIRE(s.trace.size() == 1);
    CHECK(s.trace[0].kind == Z80CycleKind::Fetch);
    CHECK(s.trace[0].address == 0x1000);
    CHECK(s.trace[0].refresh == 0x3CFE);  // R before its count
    CHECK(s.r().r == 0xFF);
    s.instruction();
    CHECK(s.r().r == 0x80);  // 7 bits counted, bit 7 kept
    CHECK(s.r().pc == 0x1002);
}

TEST_CASE("Z80 core: prefixes count R", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .r = 0});
    s.load(0x1000, {0xDD, 0x23, 0xED, 0x44, 0xCB, 0x00, 0xDD, 0xCB, 0x05, 0x06});
    s.instruction();  // INC IX
    CHECK(s.r().r == 2);
    s.instruction();  // NEG
    CHECK(s.r().r == 4);
    s.instruction();  // RLC B
    CHECK(s.r().r == 6);
    s.instruction();  // RLC (IX+5): the opcode byte is a read, not an M1
    CHECK(s.r().r == 8);
}

TEST_CASE("Z80 core: prefix chains", "[parts]") {
    SECTION("DD FD: the last prefix wins") {
        System s({.pc = 0x1000, .sp = 0x8000, .ix = 0x1111, .iy = 0x2222});
        s.load(0x1000, {0xDD, 0xFD, 0x21, 0x34, 0x12});
        CHECK(s.instruction() == 18);
        CHECK(s.r().iy == 0x1234);
        CHECK(s.r().ix == 0x1111);
        CHECK(s.r().r == 3);
    }
    SECTION("DD DD") {
        System s({.pc = 0x1000, .sp = 0x8000, .ix = 0x1111});
        s.load(0x1000, {0xDD, 0xDD, 0x21, 0x34, 0x12});
        CHECK(s.instruction() == 18);
        CHECK(s.r().ix == 0x1234);
    }
    SECTION("DD ED: the DD ignored") {
        System s({.pc = 0x1000, .sp = 0x8000, .ix = 0x1111});
        s.load(0x1000, {0xDD, 0xED, 0x6B, 0x00, 0x20});  // LD HL,(nn): HL, not IX
        s.load(0x2000, {0xCD, 0xAB});
        CHECK(s.instruction() == 24);
        CHECK(s.r().h == 0xAB);
        CHECK(s.r().l == 0xCD);
        CHECK(s.r().ix == 0x1111);
    }
}

TEST_CASE("Z80 core: ED holes", "[parts]") {
    for (const std::uint8_t op : {0x00, 0x3F, 0x80, 0x9F, 0xA4, 0xA7, 0xBC, 0xC0, 0xFF}) {
        INFO("ED " << int(op));
        const Z80Registers before{.pc = 0x1000, .sp = 0x8000, .a = 0x12, .f = 0x34, .b = 0x56};
        System s(before);
        s.load(0x1000, {0xED, op});
        CHECK(s.instruction() == 8);
        CHECK(s.trace.size() == 2);
        const Z80Registers after = s.r();
        CHECK(after.pc == 0x1002);
        CHECK(after.r == 2);
        CHECK(after.a == 0x12);
        CHECK(after.f == 0x34);
        CHECK(after.b == 0x56);
        CHECK(after.sp == 0x8000);
    }
}

TEST_CASE("Z80 core: reset", "[parts]") {
    System s({.pc = 0x1000,
              .sp = 0x8000,
              .a = 1,
              .f = 2,
              .i = 5,
              .r = 9,
              .im = 2,
              .iff1 = true,
              .iff2 = true});
    s.load(0x1000, {0x21, 0x34, 0x12});  // LD HL,nn: reset after its first cycle
    s.cycle();
    s.core.reset();
    const Z80Bus& bus = s.core.bus();
    CHECK(bus.kind == Z80CycleKind::Fetch);
    CHECK(bus.address == 0x0000);
    const Z80Registers r = s.r();
    CHECK(r.pc == 0);
    CHECK(r.i == 0);
    CHECK(r.r == 0);
    CHECK(r.im == 0);
    CHECK_FALSE(r.iff1);
    CHECK_FALSE(r.iff2);
    CHECK(r.a == 0xFF);
    CHECK(r.f == 0xFF);
    CHECK(r.sp == 0xFFFF);
}

TEST_CASE("Z80 core: HALT", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .r = 0, .im = 1, .iff1 = true, .iff2 = true});
    s.load(0x1000, {0x76});
    s.ram[0x1001] = 0x3C;  // INC A: must not run while halted
    s.instruction();
    CHECK(s.core.halted());
    for (int i = 0; i < 5; ++i) {
        CHECK(s.core.bus().kind == Z80CycleKind::Fetch);
        CHECK(s.core.bus().halted);
        CHECK(s.core.bus().address == 0x1000);  // the HALT itself, re-read (P5v)
        CHECK(s.instruction() == 4);
    }
    CHECK(s.r().pc == 0x1001);
    CHECK(s.r().a == 0);
    CHECK(s.r().r == 6);
    s.core.setInt(true);
    s.instruction();  // the last halted NOP, then the interrupt
    s.core.setInt(false);
    CHECK(s.core.bus().kind == Z80CycleKind::Acknowledge);
    CHECK(s.instruction() == 13);
    CHECK_FALSE(s.core.halted());
    CHECK(s.r().pc == 0x0038);
    CHECK(s.ram[0x7FFF] == 0x10);
    CHECK(s.ram[0x7FFE] == 0x01);  // the address after the HALT
}

TEST_CASE("Z80 core: EI shadow and DI", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .im = 1});
    s.load(0x1000, {0xFB, 0x00, 0x00, 0xF3});  // EI, NOP, NOP, DI
    s.core.setInt(true);
    s.instruction();  // EI: no interrupt after it
    CHECK(s.core.bus().kind == Z80CycleKind::Fetch);
    CHECK(s.core.bus().address == 0x1001);
    s.instruction();  // NOP: then taken
    CHECK(s.core.bus().kind == Z80CycleKind::Acknowledge);
    CHECK(s.r().pc == 0x1002);

    System masked({.pc = 0x1000, .sp = 0x8000, .im = 1, .iff1 = true, .iff2 = true});
    masked.load(0x1000, {0xF3, 0x00});  // DI, NOP
    masked.core.setInt(true);
    masked.instruction();
    masked.instruction();
    CHECK(masked.core.bus().kind == Z80CycleKind::Fetch);
    CHECK(masked.count(Z80CycleKind::Acknowledge) == 0);
}

TEST_CASE("Z80 core: interrupt modes", "[parts]") {
    SECTION("IM 1") {
        System s({.pc = 0x1000, .sp = 0x8000, .im = 1, .iff1 = true, .iff2 = true});
        s.core.setInt(true);
        s.instruction();
        s.core.setInt(false);
        CHECK(s.instruction() == 13);
        CHECK(s.r().pc == 0x0038);
        CHECK(s.r().wz == 0x0038);
        CHECK_FALSE(s.r().iff1);
        CHECK_FALSE(s.r().iff2);
        CHECK(s.ram[0x7FFE] == 0x01);
        CHECK(s.ram[0x7FFF] == 0x10);
    }
    SECTION("IM 2") {
        System s({.pc = 0x1000, .sp = 0x8000, .i = 0x20, .im = 2, .iff1 = true, .iff2 = true});
        s.vector = 0x41;  // bit 0 kept
        s.load(0x2041, {0x78, 0x56});
        s.core.setInt(true);
        s.instruction();
        s.core.setInt(false);
        CHECK(s.instruction() == 19);
        CHECK(s.r().pc == 0x5678);
        CHECK(s.ram[0x7FFE] == 0x01);
        CHECK(s.trace.back().address == 0x2042);
    }
    SECTION("IM 0, RST") {
        System s({.pc = 0x1000, .sp = 0x8000, .im = 0, .iff1 = true, .iff2 = true});
        s.vector = 0xEF;  // RST 28h
        s.core.setInt(true);
        s.instruction();
        s.core.setInt(false);
        CHECK(s.instruction() == 13);
        CHECK(s.r().pc == 0x0028);
        CHECK(s.ram[0x7FFE] == 0x01);
        CHECK(s.ram[0x7FFF] == 0x10);
    }
    SECTION("IM 0, CALL: the operands read at PC, PC not counted") {
        System s({.pc = 0x1000, .sp = 0x8000, .im = 0, .iff1 = true, .iff2 = true});
        s.vector = 0xCD;
        s.load(0x1001, {0x34});  // the device's bytes, as the bus gives them at PC
        s.core.setInt(true);
        s.instruction();  // NOP at 1000; PC 1001
        s.core.setInt(false);
        s.ram[0x1001] = 0x34;
        const std::size_t first = s.trace.size();
        // Both operand reads at 1001: give $34 then $12.
        const long start = s.tStates;
        int reads = 0;
        do {
            const Z80Bus bus = s.core.bus();
            if (bus.kind == Z80CycleKind::Read) {
                CHECK(bus.address == 0x1001);
                s.ram[0x1001] = reads++ == 0 ? 0x34 : 0x12;
            }
            s.cycle();
        } while (!s.core.instructionEnded());
        CHECK(s.tStates - start == 19);
        CHECK(reads == 2);
        CHECK(s.trace[first].kind == Z80CycleKind::Acknowledge);
        CHECK(s.r().pc == 0x1234);
        CHECK(s.ram[0x7FFE] == 0x01);  // the interrupted PC
        CHECK(s.ram[0x7FFF] == 0x10);
    }
}

TEST_CASE("Z80 core: NMI", "[parts]") {
    SECTION("once per edge, over INT; IFF1 cleared, IFF2 kept; RETN restores") {
        System s({.pc = 0x1000, .sp = 0x8000, .im = 1, .iff1 = true, .iff2 = true});
        s.load(0x0066, {0xED, 0x45});  // RETN
        s.core.setInt(true);
        s.core.setNmi(true);
        s.instruction();  // NOP, then the NMI (not the INT)
        CHECK(s.core.bus().kind == Z80CycleKind::Fetch);
        CHECK(s.core.bus().address == 0x1001);
        CHECK(s.instruction() == 11);
        CHECK(s.r().pc == 0x0066);
        CHECK_FALSE(s.r().iff1);
        CHECK(s.r().iff2);
        s.core.setInt(false);
        // RETN, then a NOP, NMI still held and given at every cycle (as the Part does): not
        // taken again.
        for (const std::uint16_t pc : {0x1001, 0x1002}) {
            do {
                s.core.setNmi(true);
                s.cycle();
            } while (!s.core.instructionEnded());
            CHECK(s.r().pc == pc);
            CHECK(s.r().iff1);
        }
    }
    SECTION("a short pulse between two steps is kept") {
        System s({.pc = 0x1000, .sp = 0x8000});
        s.load(0x1000, {0x21, 0x34, 0x12});  // LD HL,nn
        s.cycle();
        s.core.setNmi(true);
        s.cycle();
        s.core.setNmi(false);
        s.cycle();
        CHECK(s.core.instructionEnded());
        s.instruction();
        CHECK(s.r().pc == 0x0066);
    }
    SECTION("taken after EI") {
        System s({.pc = 0x1000, .sp = 0x8000});
        s.load(0x1000, {0xFB});
        s.core.setNmi(true);
        s.instruction();
        s.instruction();
        CHECK(s.r().pc == 0x0066);
    }
}

TEST_CASE("Z80 core: no interrupt between a prefix and its opcode", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .im = 1, .iff1 = true, .iff2 = true});
    s.load(0x1000, {0xDD, 0x21, 0x34, 0x12});
    s.core.setInt(true);
    s.cycle();  // the DD fetch
    CHECK_FALSE(s.core.instructionEnded());
    CHECK(s.core.bus().kind == Z80CycleKind::Fetch);
    s.instruction();
    CHECK(s.r().ix == 0x1234);
    CHECK(s.core.bus().kind == Z80CycleKind::Acknowledge);
}

TEST_CASE("Z80 core: LDIR interrupted", "[parts]") {
    System s({.pc = 0x1000,
              .sp = 0x8000,
              .b = 0,
              .c = 3,
              .d = 0x30,
              .e = 0,
              .h = 0x20,
              .l = 0,
              .im = 1,
              .iff1 = true,
              .iff2 = true});
    s.load(0x1000, {0xED, 0xB0});
    s.load(0x2000, {1, 2, 3});
    CHECK(s.instruction() == 21);  // one repeat; INT not yet asserted
    CHECK(s.r().pc == 0x1000);
    s.core.setInt(true);
    s.instruction();
    s.core.setInt(false);
    CHECK(s.core.bus().kind == Z80CycleKind::Acknowledge);
    s.instruction();
    CHECK(s.ram[0x7FFE] == 0x00);  // the pushed PC is the LDIR's
    CHECK(s.ram[0x7FFF] == 0x10);
    CHECK(s.ram[0x3000] == 1);
    CHECK(s.ram[0x3001] == 2);
    CHECK(s.r().c == 1);
}

TEST_CASE("Z80 core: LD A,I interrupted loses P/V", "[parts]") {
    System s({.pc = 0x1000, .sp = 0x8000, .im = 1, .iff1 = true, .iff2 = true});
    s.load(0x1000, {0xED, 0x57, 0xED, 0x57});
    s.instruction();
    CHECK((s.r().f & 0x04) != 0);  // P/V = IFF2
    s.core.setInt(true);
    s.instruction();
    CHECK((s.r().f & 0x04) == 0);  // the interrupt accepted after it
}
