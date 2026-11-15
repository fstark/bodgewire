// The 6522's logic cycle by cycle, without the simulator (P5n, BW2001_CHIPS_DESIGN.md §6).

#include "parts/mos6522_core.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

// Registers.
constexpr unsigned kOrb = 0, kOra = 1, kDdrb = 2, kDdra = 3, kT1L = 4, kT1H = 5, kT1LL = 6,
                   kT1LH = 7, kT2L = 8, kT2H = 9, kSr = 0xA, kAcr = 0xB, kPcr = 0xC, kIfr = 0xD,
                   kIer = 0xE, kOraNoHandshake = 0xF;

// One VIA and its input pins; each call is one φ2 cycle.
struct Via {
    Mos6522Core core;
    Mos6522Core::Inputs in;

    // Reset, then one cycle so the inputs have been sampled once (edges count from there).
    Via() {
        core.reset();
        idle();
    }
    std::uint8_t read(unsigned reg) {
        core.rise(in);
        const std::uint8_t v = core.read(reg);
        core.fall();
        return v;
    }
    void write(unsigned reg, std::uint8_t v) {
        core.rise(in);
        core.write(reg, v);
        core.fall();
    }
    void idle(int cycles = 1) {
        for (int i = 0; i < cycles; ++i) {
            core.rise(in);
            core.fall();
        }
    }
    // An idle cycle; T1 as a read in it would see it (between rise and fall).
    std::uint16_t t1Seen() {
        core.rise(in);
        const std::uint16_t v = core.t1();
        core.fall();
        return v;
    }
    std::uint16_t t2Seen() {
        core.rise(in);
        const std::uint16_t v = core.t2();
        core.fall();
        return v;
    }
    Mos6522Core::Outputs out() const { return core.outputs(); }
};

}  // namespace

TEST_CASE("6522 core: registers read back", "[parts]") {
    Via v;
    v.write(kDdra, 0x5A);
    v.write(kDdrb, 0xA5);
    v.write(kAcr, 0x03);
    v.write(kPcr, 0x11);
    v.write(kT1LL, 0x34);
    v.write(kT1LH, 0x12);
    CHECK(v.read(kDdra) == 0x5A);
    CHECK(v.read(kDdrb) == 0xA5);
    CHECK(v.read(kAcr) == 0x03);
    CHECK(v.read(kPcr) == 0x11);
    CHECK(v.read(kT1LL) == 0x34);
    CHECK(v.read(kT1LH) == 0x12);
    v.write(kIer, 0x80 | 0x42);
    CHECK(v.read(kIer) == 0xC2);  // bit 7 reads 1
    v.write(kIer, 0x02);          // bit 7 0: clear
    CHECK(v.read(kIer) == 0xC0);
}

TEST_CASE("6522 core: ports read the pins, the latch on output bits of B", "[parts]") {
    Via v;
    v.in.pa = 0x3C;
    v.in.pb = 0x0F;
    v.write(kDdrb, 0xF0);
    v.write(kOrb, 0xA5);
    CHECK(v.read(kOra) == 0x3C);
    CHECK(v.read(kOraNoHandshake) == 0x3C);
    CHECK(v.read(kOrb) == 0xAF);  // ORB high nibble, pins low nibble
    v.write(kDdra, 0xFF);
    v.write(kOra, 0x81);
    CHECK(v.out().paDrive == 0xFF);
    CHECK(v.out().paLevel == 0x81);
    CHECK(v.out().pbDrive == 0xF0);
    CHECK(v.out().pbLevel == 0xA5);
}

TEST_CASE("6522 core: T1 one-shot counts N .. 0, $FFFF, then reloads; one flag", "[parts]") {
    Via v;
    v.write(kIer, 0xC0);
    v.write(kT1L, 3);
    v.write(kT1H, 0);
    const std::vector<std::uint16_t> expected{3, 2, 1, 0, 0xFFFF, 3, 2, 1, 0, 0xFFFF, 3};
    std::vector<std::uint16_t> seen;
    std::vector<bool> flagged, irq;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        v.core.rise(v.in);
        seen.push_back(v.core.t1());
        flagged.push_back((v.core.ifr() & 0x40) != 0);
        v.core.fall();
        irq.push_back(v.out().irq);
    }
    CHECK(seen == expected);
    CHECK_FALSE(flagged[3]);
    CHECK(flagged[4]);  // the $FFFF cycle reads the flag
    CHECK_FALSE(irq[2]);
    CHECK(irq[3]);  // IRQ from the fall of the 0 cycle: N + 1 after the write
    v.read(kT1L);   // clears it
    CHECK_FALSE(v.out().irq);
    v.idle(10);
    CHECK((v.core.ifr() & 0x40) == 0);  // one-shot: no second flag
}

TEST_CASE("6522 core: T1 free-running, period N + 2, PB7 square wave", "[parts]") {
    Via v;
    v.write(kAcr, 0xC0);  // free-running, PB7 on
    v.write(kT1L, 3);
    v.write(kT1H, 0);
    CHECK(v.out().pb7Timer);
    CHECK((v.out().pbDrive & 0x80) != 0);  // whatever DDRB bit 7 says
    CHECK((v.out().pbLevel & 0x80) == 0);  // low from the write
    std::vector<int> toggles;
    bool last = false;
    for (int cycle = 1; cycle <= 20; ++cycle) {
        v.idle();
        const bool level = (v.out().pbLevel & 0x80) != 0;
        if (level != last) {
            toggles.push_back(cycle);
            last = level;
        }
    }
    REQUIRE(toggles.size() >= 3);
    CHECK(toggles[0] == 4);  // the fall of the 0 cycle (N + 1 cycles after the write)
    CHECK(toggles[1] - toggles[0] == 5);
    CHECK(toggles[2] - toggles[1] == 5);
}

TEST_CASE("6522 core: T1 one-shot PB7 pulse, and a latch write changes the next period",
          "[parts]") {
    SECTION("one-shot PB7: low from the write to the timeout") {
        Via v;
        v.write(kAcr, 0x80);
        v.write(kT1L, 2);
        v.write(kT1H, 0);
        CHECK((v.out().pbLevel & 0x80) == 0);
        v.idle(2);
        CHECK((v.out().pbLevel & 0x80) == 0);
        v.idle();
        CHECK((v.out().pbLevel & 0x80) != 0);
        v.idle(10);
        CHECK((v.out().pbLevel & 0x80) != 0);
    }
    SECTION("latch only") {
        Via v;
        v.write(kAcr, 0x40);
        v.write(kT1L, 3);
        v.write(kT1H, 0);
        v.write(kT1LL, 6);  // cycle 1: the counter still runs from 3
        std::vector<int> wraps;
        for (int cycle = 2; cycle <= 25; ++cycle) {
            if (v.t1Seen() == 0xFFFF) {
                wraps.push_back(cycle);
            }
        }
        REQUIRE(wraps.size() >= 3);
        CHECK(wraps[0] == 5);
        CHECK(wraps[1] - wraps[0] == 8);  // N + 2 with the new latch
        CHECK(wraps[2] - wraps[1] == 8);
    }
}

TEST_CASE("6522 core: T2 one-shot and pulse counting", "[parts]") {
    SECTION("one-shot: no reload, one flag") {
        Via v;
        v.write(kT2L, 3);
        v.write(kT2H, 0);
        std::vector<std::uint16_t> seen;
        for (int i = 0; i < 7; ++i) {
            seen.push_back(v.t2Seen());
        }
        CHECK(seen == std::vector<std::uint16_t>{3, 2, 1, 0, 0xFFFF, 0xFFFE, 0xFFFD});
        CHECK((v.core.ifr() & 0x20) != 0);
        v.read(kT2L);
        CHECK((v.core.ifr() & 0x20) == 0);
        v.idle(0x10000);
        CHECK((v.core.ifr() & 0x20) == 0);
    }
    SECTION("counting PB6 falls: the flag on reaching 0") {
        Via v;
        v.write(kAcr, 0x20);
        v.write(kT2L, 3);
        v.write(kT2H, 0);
        for (int pulse = 1; pulse <= 3; ++pulse) {
            v.in.pb = 0xBF;  // PB6 low
            v.idle();
            v.in.pb = 0xFF;
            v.idle(2);
            CHECK(v.core.t2() == 3 - pulse);
            CHECK(((v.core.ifr() & 0x20) != 0) == (pulse == 3));
        }
    }
}

TEST_CASE("6522 core: shift register directions per mode", "[parts]") {
    // Mode: CB1 an output, CB2 an output.
    const std::vector<std::pair<bool, bool>> expected{{false, false}, {true, false}, {true, false},
                                                      {false, false}, {true, true},  {true, true},
                                                      {true, true},   {false, true}};
    for (unsigned mode = 0; mode < 8; ++mode) {
        INFO("mode " << mode);
        Via v;
        v.write(kAcr, static_cast<std::uint8_t>(mode << 2));
        CHECK(v.out().cb1Drive == expected[mode].first);
        CHECK(v.out().cb2Drive == expected[mode].second);
    }
}

TEST_CASE("6522 core: shift out under φ2, 2 cycles a bit, the flag after 8", "[parts]") {
    Via v;
    v.write(kAcr, 0x18);  // mode 6
    v.write(kSr, 0xA5);
    std::vector<int> bits;
    bool cb1 = true;
    int cycles = 0;
    while ((v.core.ifr() & 0x04) == 0 && cycles < 40) {
        v.idle();
        ++cycles;
        const auto o = v.out();
        if (cb1 && !o.cb1) {
            bits.push_back(o.cb2 ? 1 : 0);
        }
        cb1 = o.cb1;
    }
    CHECK(bits == std::vector<int>{1, 0, 1, 0, 0, 1, 0, 1});
    CHECK(cycles == 16);
    CHECK(v.out().cb1);
    v.idle(6);
    CHECK(v.out().cb1);  // stopped, high
}

TEST_CASE("6522 core: shift in under φ2 and under T2", "[parts]") {
    SECTION("mode 2: CB2 taken in as CB1 rises") {
        Via v;
        v.write(kAcr, 0x08);
        v.read(kSr);  // starts it
        const std::vector<bool> pattern{true, true, false, true, false, false, true, false};
        std::size_t next = 0;
        int cycles = 0;
        while ((v.core.ifr() & 0x04) == 0 && cycles < 40) {
            v.in.cb2 = next < pattern.size() ? pattern[next] : false;
            const bool before = v.out().cb1;
            v.idle();
            ++cycles;
            if (!before && v.out().cb1) {
                ++next;
            }
        }
        CHECK(cycles == 16);
        CHECK(v.core.sr() == 0xD2);
    }
    SECTION("mode 1: CB1 half period N + 2") {
        Via v;
        v.write(kT2L, 1);  // half period 3
        v.write(kAcr, 0x04);
        v.read(kSr);
        int cycles = 0;
        while ((v.core.ifr() & 0x04) == 0 && cycles < 100) {
            v.idle();
            ++cycles;
        }
        CHECK(cycles == 48);  // 8 bits x 2 x 3
    }
}

TEST_CASE("6522 core: shift out under T2: mode 5 flags, mode 4 recirculates", "[parts]") {
    SECTION("mode 5") {
        Via v;
        v.write(kT2L, 2);  // half period 4
        v.write(kAcr, 0x14);
        v.write(kSr, 0x0F);
        int cycles = 0;
        while ((v.core.ifr() & 0x04) == 0 && cycles < 100) {
            v.idle();
            ++cycles;
        }
        CHECK(cycles == 64);
        CHECK(v.core.sr() == 0x0F);  // rotated through
    }
    SECTION("mode 4: on and on, no flag") {
        Via v;
        v.write(kT2L, 2);
        v.write(kAcr, 0x10);
        v.write(kSr, 0x0F);
        std::vector<int> bits;
        bool cb1 = true;
        for (int i = 0; i < 160; ++i) {
            v.idle();
            const auto o = v.out();
            if (cb1 && !o.cb1) {
                bits.push_back(o.cb2 ? 1 : 0);
            }
            cb1 = o.cb1;
        }
        CHECK((v.core.ifr() & 0x04) == 0);
        REQUIRE(bits.size() >= 16);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK(bits[i] == ((i % 8) >= 4 ? 1 : 0));
        }
    }
}

TEST_CASE("6522 core: shift with an outside CB1 clock", "[parts]") {
    SECTION("mode 3: in on CB1 rising") {
        Via v;
        v.write(kAcr, 0x0C);
        v.read(kSr);
        for (int bit = 0; bit < 8; ++bit) {
            v.in.cb2 = bit % 3 == 0;
            v.in.cb1 = false;
            v.idle();
            v.in.cb1 = true;
            v.idle();
        }
        CHECK(v.core.sr() == 0b10010010);
        CHECK((v.core.ifr() & 0x04) != 0);
    }
    SECTION("mode 7: out on CB1 falling, flags every 8 and goes on") {
        Via v;
        v.write(kAcr, 0x1C);
        v.write(kSr, 0x81);
        std::vector<int> bits;
        for (int bit = 0; bit < 16; ++bit) {
            v.in.cb1 = false;
            v.idle();
            bits.push_back(v.out().cb2 ? 1 : 0);
            v.in.cb1 = true;
            v.idle();
            if (bit == 7) {
                CHECK((v.core.ifr() & 0x04) != 0);
                v.write(kIfr, 0x04);
            }
        }
        CHECK(bits == std::vector<int>{1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1});
        CHECK((v.core.ifr() & 0x04) != 0);
    }
    SECTION("mode 0: nothing shifts") {
        Via v;
        v.write(kSr, 0x81);
        for (int i = 0; i < 8; ++i) {
            v.in.cb1 = false;
            v.idle();
            v.in.cb1 = true;
            v.idle();
        }
        CHECK(v.core.sr() == 0x81);
        CHECK((v.core.ifr() & 0x04) == 0);
    }
}

TEST_CASE("6522 core: input latching on C1 edges", "[parts]") {
    Via v;
    v.write(kAcr, 0x03);  // latch A and B
    v.in.pa = 0x12;
    v.in.pb = 0x56;
    v.in.ca1 = false;  // falling edge (PCR bit 0 is 0)
    v.in.cb1 = false;
    v.idle();
    v.in.pa = 0x34;
    v.in.pb = 0x78;
    CHECK(v.read(kOra) == 0x12);
    CHECK(v.read(kOrb) == 0x56);
    v.write(kAcr, 0x00);
    CHECK(v.read(kOra) == 0x34);
}

TEST_CASE("6522 core: CA2 input modes and their flags", "[parts]") {
    SECTION("mode 000: falling, cleared by a register 1 access") {
        Via v;
        v.in.ca2 = false;
        v.idle();
        CHECK((v.core.ifr() & 0x01) != 0);
        v.read(kOraNoHandshake);
        CHECK((v.core.ifr() & 0x01) != 0);  // register F clears nothing
        v.read(kOra);
        CHECK((v.core.ifr() & 0x01) == 0);
    }
    SECTION("mode 001: independent, a register 1 access leaves it") {
        Via v;
        v.write(kPcr, 0x02);
        v.in.ca2 = false;
        v.idle();
        v.read(kOra);
        CHECK((v.core.ifr() & 0x01) != 0);
        v.write(kIfr, 0x01);
        CHECK((v.core.ifr() & 0x01) == 0);
    }
    SECTION("mode 010: rising") {
        Via v;
        v.write(kPcr, 0x04);
        v.in.ca2 = false;
        v.idle();
        CHECK((v.core.ifr() & 0x01) == 0);
        v.in.ca2 = true;
        v.idle();
        CHECK((v.core.ifr() & 0x01) != 0);
    }
    SECTION("CA1: the flag, cleared by register 1") {
        Via v;
        v.in.ca1 = false;
        v.idle();
        CHECK((v.core.ifr() & 0x02) != 0);
        v.write(kOra, 0);
        CHECK((v.core.ifr() & 0x02) == 0);
    }
}

TEST_CASE("6522 core: CA2 and CB2 outputs", "[parts]") {
    SECTION("CA2 handshake: low after a read's fall, high on CA1") {
        Via v;
        v.write(kPcr, 0x08);
        CHECK(v.out().ca2Drive);
        CHECK(v.out().ca2);
        v.core.rise(v.in);
        v.core.read(kOra);
        CHECK(v.out().ca2);  // not yet
        v.core.fall();
        CHECK_FALSE(v.out().ca2);
        v.idle(3);
        CHECK_FALSE(v.out().ca2);
        v.in.ca1 = false;  // active edge
        v.core.rise(v.in);
        CHECK(v.out().ca2);
        v.core.fall();
    }
    SECTION("CA2 pulse after a read: fall to fall; after a write: rise to rise") {
        Via v;
        v.write(kPcr, 0x0A);
        v.read(kOra);
        CHECK_FALSE(v.out().ca2);
        v.core.rise(v.in);
        CHECK_FALSE(v.out().ca2);
        v.core.fall();
        CHECK(v.out().ca2);
        v.write(kOra, 0);
        CHECK(v.out().ca2);  // a write strobes from the next rise
        v.core.rise(v.in);
        CHECK_FALSE(v.out().ca2);
        v.core.fall();
        CHECK_FALSE(v.out().ca2);
        v.core.rise(v.in);
        CHECK(v.out().ca2);
        v.core.fall();
    }
    SECTION("register F does not strobe") {
        Via v;
        v.write(kPcr, 0x0A);
        v.read(kOraNoHandshake);
        v.write(kOraNoHandshake, 0);
        v.idle();
        CHECK(v.out().ca2);
    }
    SECTION("manual levels") {
        Via v;
        v.write(kPcr, 0xCC);
        CHECK_FALSE(v.out().ca2);
        CHECK_FALSE(v.out().cb2);
        v.write(kPcr, 0xEE);
        CHECK(v.out().ca2);
        CHECK(v.out().cb2);
    }
    SECTION("CB2 handshake after a write of ORB, high on CB1") {
        Via v;
        v.write(kPcr, 0x80);
        v.read(kOrb);
        v.idle();
        CHECK(v.out().cb2);  // a read does not strobe CB2
        v.write(kOrb, 0);
        CHECK(v.out().cb2);
        v.core.rise(v.in);
        CHECK_FALSE(v.out().cb2);
        v.core.fall();
        v.in.cb1 = false;
        v.idle();
        CHECK(v.out().cb2);
    }
}

TEST_CASE("6522 core: IFR bit 7, IER, IRQ", "[parts]") {
    Via v;
    v.in.ca1 = false;
    v.idle();
    CHECK((v.core.ifr() & 0x02) != 0);
    CHECK_FALSE(v.out().irq);
    CHECK(v.read(kIfr) == 0x02);  // bit 7 only for enabled flags
    v.write(kIer, 0x82);
    CHECK(v.out().irq);
    CHECK(v.read(kIfr) == 0x82);
    v.write(kIer, 0x02);
    CHECK_FALSE(v.out().irq);
    v.write(kIer, 0x82);
    v.write(kIfr, 0x02);
    CHECK_FALSE(v.out().irq);
    CHECK(v.read(kIfr) == 0x00);
}

TEST_CASE("6522 core: reset keeps T1, T2 and SR", "[parts]") {
    Via v;
    v.write(kDdra, 0xFF);
    v.write(kAcr, 0x40);
    v.write(kT1L, 0x34);
    v.write(kT1H, 0x12);
    v.write(kT2L, 0x78);
    v.write(kT2H, 0x56);
    v.write(kSr, 0x9A);
    const std::uint16_t t1 = v.core.t1();
    const std::uint16_t t2 = v.core.t2();
    v.core.reset();
    CHECK(v.core.t1() == t1);
    CHECK(v.core.t2() == t2);
    CHECK(v.core.sr() == 0x9A);
    CHECK(v.read(kDdra) == 0);
    CHECK(v.read(kAcr) == 0);
    CHECK(v.read(kT1LL) == 0x34);
    CHECK(v.read(kT1LH) == 0x12);
}
