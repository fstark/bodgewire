#include "parts/mos6520.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "bus_bench.hpp"
#include "parts/catalogue.hpp"
#include "parts/delays.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using namespace delays;

// Registers by RS1 RS0.
constexpr unsigned kRegA = 0, kCra = 1, kRegB = 2, kCrb = 3;

const BusPins kPins{Mos6520::kPhi2,
                    Mos6520::kRw,
                    Mos6520::kResN,
                    Mos6520::kData,
                    {Mos6520::kRs0, Mos6520::kRs1},
                    {{Mos6520::kCs0, true}, {Mos6520::kCs1, true}, {Mos6520::kCs2N, false}}};

BusCycle Reset() { return {0, true, 0, false, true}; }

// Reset, then `script`, then `idle` idle cycles.
std::vector<BusCycle> Script(std::vector<BusCycle> script, std::size_t idle = 4) {
    script.insert(script.begin(), Reset());
    script.insert(script.end(), idle, BusIdle());
    return script;
}

}  // namespace

TEST_CASE("6520: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("6520");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-40");
    const auto pins = Mos6520::Pins();
    REQUIRE(pins.size() == 40);
    const std::array<std::string_view, 40> names{
        "GND", "PA0", "PA1",   "PA2",   "PA3",  "PA4", "PA5",    "PA6",    "PA7", "PB0",
        "PB1", "PB2", "PB3",   "PB4",   "PB5",  "PB6", "PB7",    "CB1",    "CB2", "VCC",
        "RW",  "CS0", "CS2_N", "CS1",   "PHI2", "D7",  "D6",     "D5",     "D4",  "D3",
        "D2",  "D1",  "D0",    "RES_N", "RS1",  "RS0", "IRQB_N", "IRQA_N", "CA2", "CA1"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(names[i]);
        CHECK(pins[i].name == names[i]);
        if (names[i] == "GND" || names[i] == "VCC") {
            CHECK(pins[i].kind == PinKind::Power);
            continue;
        }
        CHECK(pins[i].family == Family::Mos);
        const bool irq = names[i].starts_with("IRQ");
        CHECK(pins[i].sampled == (names[i] != "PHI2" && !irq));
        PinKind kind = PinKind::Input;
        if (names[i].starts_with("PA") || names[i] == "CA2") {
            kind = PinKind::PullUp;
        } else if (names[i].starts_with("PB") || names[i] == "CB2" || names[i].starts_with("D")) {
            kind = PinKind::Bidirectional;
        } else if (irq) {
            kind = PinKind::OpenCollector;
        }
        CHECK(pins[i].kind == kind);
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        CHECK(pins[Mos6520::kData[bit]].name == "D" + std::to_string(bit));
        CHECK(pins[Mos6520::kPortA[bit]].name == "PA" + std::to_string(bit));
        CHECK(pins[Mos6520::kPortB[bit]].name == "PB" + std::to_string(bit));
    }
}

TEST_CASE("6520: registers by RS1, RS0 and CR bit 2", "[parts]") {
    // PB0-PB3 driven 0011 from outside; PB4-PB7 outputs.
    BusOutside outside{
        {"PB0", {One, {}}}, {"PB1", {One, {}}}, {"PB2", {Zero, {}}}, {"PB3", {Zero, {}}}};
    const auto script = Script({BusWrite(kRegA, 0x0F),  // CRA bit 2 is 0: DDRA
                                BusWrite(kCra, 0x04), BusWrite(kRegA, 0x05), BusRead(kCra),
                                BusRead(kRegA),  // the PA pins: 0101 out, pull-ups in
                                BusWrite(kCra, 0x00), BusRead(kRegA),  // DDRA
                                BusWrite(kRegB, 0xF0), BusWrite(kCrb, 0x04), BusWrite(kRegB, 0xA5),
                                BusRead(kRegB),  // ORB on outputs, the pins on inputs
                                BusRead(kCrb), BusWrite(kCrb, 0x00), BusRead(kRegB)});
    BusBench b;
    b.build<Mos6520>(kPins, script, outside);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 6);
    CHECK(reads[0] == 0x04u);
    CHECK(reads[1] == 0xF5u);
    CHECK(reads[2] == 0x0Fu);
    CHECK(reads[3] == 0xA3u);
    CHECK(reads[4] == 0x04u);
    CHECK(reads[5] == 0xF0u);
    CHECK(b.level(sim, "PA0") == One);
    CHECK(b.level(sim, "PA1") == Zero);
    CHECK(b.level(sim, "PA7") == One);  // input: the pull-up
    CHECK(b.level(sim, "PB7") == One);
    CHECK(b.level(sim, "PB4") == Zero);
}

TEST_CASE("6520: a port A read gives the pins, a port B read the register", "[parts]") {
    // PA0 and PB0 outputs at 1, both held low from outside.
    BusOutside outside{{"PA0", {Zero, {}}}, {"PB0", {Zero, {}}}};
    const auto script = Script({BusWrite(kRegA, 0x01), BusWrite(kRegB, 0x01), BusWrite(kCra, 0x04),
                                BusWrite(kCrb, 0x04), BusWrite(kRegA, 0x01), BusWrite(kRegB, 0x01),
                                BusIdle(), BusIdle(), BusRead(kRegA), BusRead(kRegB)});
    BusBench b;
    b.build<Mos6520>(kPins, script, outside);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 2);
    REQUIRE(reads[0].has_value());
    CHECK((*reads[0] & 1) == 0);  // PA0 the pin: low
    REQUIRE(reads[1].has_value());
    CHECK((*reads[1] & 1) == 1);  // PB0 the register: 1
}

TEST_CASE("6520: inputs after reset: PA pulled up, PB floating, IRQs released", "[parts]") {
    const auto script = Script({});
    BusBench b;
    b.build<Mos6520>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(3);
    sim.runUntil(BusFall(0) + 2_us);
    for (unsigned bit = 0; bit < 8; ++bit) {
        CHECK(b.level(sim, "PA" + std::to_string(bit)) == One);
        CHECK(b.level(sim, "PB" + std::to_string(bit)) == Z);
    }
    CHECK(b.level(sim, "CA2") == One);
    CHECK(b.level(sim, "CB2") == Z);
    CHECK(b.level(sim, "IRQA_N") == Z);
    CHECK(b.level(sim, "IRQB_N") == Z);
}

TEST_CASE("6520: reset clears every register", "[parts]") {
    const auto script =
        Script({BusWrite(kRegA, 0xFF), BusWrite(kRegB, 0xFF), BusWrite(kCra, 0x3F),
                BusWrite(kCrb, 0x3F), BusWrite(kRegA, 0x55), BusWrite(kRegB, 0x55), Reset(),
                BusRead(kCra), BusRead(kCrb), BusRead(kRegA), BusRead(kRegB)});
    BusBench b;
    b.build<Mos6520>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 4);
    CHECK(reads[0] == 0u);
    CHECK(reads[1] == 0u);
    CHECK(reads[2] == 0u);  // DDRA
    CHECK(reads[3] == 0u);  // DDRB
    CHECK(b.level(sim, "PA0") == One);
    CHECK(b.level(sim, "PB0") == Z);
}

TEST_CASE("6520: bus and port delays", "[parts]") {
    // PA0 an output at 1; cycle 6 writes ORA = 0; cycle 8 reads CRA.
    const auto script =
        Script({BusWrite(kRegA, 0x01), BusWrite(kCra, 0x04), BusWrite(kRegA, 0x01), BusIdle(),
                BusIdle(), BusWrite(kRegA, 0x00), BusIdle(), BusRead(kCra)});
    BusBench b;
    b.build<Mos6520>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(BusFall(6) + kMos6520Port - 1);
    CHECK(b.level(sim, "PA0") == One);
    sim.runUntil(BusFall(6) + kMos6520Port);
    CHECK(b.level(sim, "PA0") == Zero);
    sim.runUntil(BusRise(8) + kMos6520DataOut - 1);
    CHECK_FALSE(b.bus(sim).has_value());
    sim.runUntil(BusRise(8) + kMos6520DataOut);
    CHECK(b.bus(sim) == 0x04u);
    sim.runUntil(BusFall(8) + kMos6520DataHold - 1);
    CHECK(b.bus(sim) == 0x04u);
    sim.runUntil(BusFall(8) + kMos6520DataHold);
    CHECK_FALSE(b.bus(sim).has_value());
}

TEST_CASE("6520: C1 edges set the flags; a port read clears them", "[parts]") {
    SECTION("CA1 falling, IRQ enabled") {
        // CRA = $05: IRQ on, falling edge, ORA. CA1 falls at 3.2 µs (seen at the 3.5 µs rise).
        BusOutside outside{{"CA1", {One, {{3200_ns, Zero}}}}};
        const auto script =
            Script({BusWrite(kCra, 0x05), BusIdle(), BusIdle(), BusRead(kCra), BusWrite(kCra, 0x01),
                    BusRead(kRegA),  // DDRA: kept
                    BusRead(kCra), BusWrite(kCra, 0x05), BusRead(kRegA), BusRead(kCra)});
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusRise(3) + kMos6520Irq - 1);
        CHECK(b.level(sim, "IRQA_N") != Zero);
        sim.runUntil(BusRise(3) + kMos6520Irq);
        CHECK(b.level(sim, "IRQA_N") == Zero);
        CHECK(b.level(sim, "IRQB_N") != Zero);
        const auto reads = RunReads(b, sim, script);
        REQUIRE(reads.size() == 5);
        CHECK(reads[0] == 0x85u);
        CHECK(reads[2] == 0x81u);  // a DDRA read does not clear
        CHECK(reads[4] == 0x05u);  // the ORA read did
        CHECK(b.level(sim, "IRQA_N") != Zero);
    }
    SECTION("CA1 rising per CRA bit 1; no IRQ without bit 0") {
        BusOutside outside{{"CA1", {Zero, {{3200_ns, One}}}}};
        const auto script = Script({BusWrite(kCra, 0x06), BusIdle(), BusIdle(), BusRead(kCra)});
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        const auto reads = RunReads(b, sim, script);
        REQUIRE(reads.size() == 1);
        CHECK(reads[0] == 0x86u);
        CHECK(b.level(sim, "IRQA_N") != Zero);
    }
    SECTION("the wrong edge sets nothing") {
        BusOutside outside{{"CA1", {Zero, {{3200_ns, One}}}}};
        const auto script = Script({BusWrite(kCra, 0x05), BusIdle(), BusIdle(), BusRead(kCra)});
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        const auto reads = RunReads(b, sim, script);
        REQUIRE(reads.size() == 1);
        CHECK(reads[0] == 0x05u);
    }
    SECTION("CB1 sets side B only") {
        BusOutside outside{{"CB1", {One, {{3200_ns, Zero}}}}};
        const auto script = Script({BusWrite(kCrb, 0x05), BusIdle(), BusIdle(), BusRead(kCrb),
                                    BusRead(kCra), BusRead(kRegB), BusRead(kCrb)});
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusRise(3) + kMos6520Irq);
        CHECK(b.level(sim, "IRQB_N") == Zero);
        CHECK(b.level(sim, "IRQA_N") != Zero);
        const auto reads = RunReads(b, sim, script);
        REQUIRE(reads.size() == 4);
        CHECK(reads[0] == 0x85u);
        CHECK(reads[1] == 0x00u);
        CHECK(reads[3] == 0x05u);
        CHECK(b.level(sim, "IRQB_N") != Zero);
    }
}

TEST_CASE("6520: C2 as an input", "[parts]") {
    // CRA = $1C: C2 input, rising edge, its IRQ on, ORA. CA2 rises at 3.2 µs.
    BusOutside outside{{"CA2", {Zero, {{3200_ns, One}}}}};
    const auto script = Script(
        {BusWrite(kCra, 0x1C), BusIdle(), BusIdle(), BusRead(kCra), BusRead(kRegA), BusRead(kCra)});
    BusBench b;
    b.build<Mos6520>(kPins, script, outside);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(BusRise(3) + kMos6520Irq);
    CHECK(b.level(sim, "IRQA_N") == Zero);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 3);
    CHECK(reads[0] == 0x5Cu);
    CHECK(reads[2] == 0x1Cu);
}

TEST_CASE("6520: CA2 handshake and pulse after a port A read", "[parts]") {
    SECTION("handshake: low after the read's fall, high again on the CA1 edge") {
        // CRA = $24: CA2 handshake, CA1 falling, ORA. Read ORA in cycle 2; CA1 falls at 5.2 µs.
        BusOutside outside{{"CA1", {One, {{5200_ns, Zero}}}}};
        const auto script = Script({BusWrite(kCra, 0x24), BusRead(kRegA)}, 6);
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusFall(2) + kMos6520Control - 1);
        CHECK(b.level(sim, "CA2") == One);
        sim.runUntil(BusFall(2) + kMos6520Control);
        CHECK(b.level(sim, "CA2") == Zero);
        sim.runUntil(BusRise(5) + kMos6520Control - 1);
        CHECK(b.level(sim, "CA2") == Zero);
        sim.runUntil(BusRise(5) + kMos6520Control);
        CHECK(b.level(sim, "CA2") == One);
    }
    SECTION("pulse: low for one cycle") {
        const auto script = Script({BusWrite(kCra, 0x2C), BusRead(kRegA)}, 4);
        BusBench b;
        b.build<Mos6520>(kPins, script);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusFall(2) + kMos6520Control);
        CHECK(b.level(sim, "CA2") == Zero);
        sim.runUntil(BusFall(3) + kMos6520Control - 1);
        CHECK(b.level(sim, "CA2") == Zero);
        sim.runUntil(BusFall(3) + kMos6520Control);
        CHECK(b.level(sim, "CA2") == One);
    }
}

TEST_CASE("6520: CB2 handshake and pulse after a port B write", "[parts]") {
    SECTION("handshake: low from the next rise, high on the CB1 edge") {
        // CRB = $24: CB2 handshake, CB1 falling. Write ORB in cycle 2; CB1 falls at 5.2 µs.
        BusOutside outside{{"CB1", {One, {{5200_ns, Zero}}}}};
        const auto script = Script({BusWrite(kCrb, 0x24), BusWrite(kRegB, 0x00)}, 6);
        BusBench b;
        b.build<Mos6520>(kPins, script, outside);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusRise(3) + kMos6520Control - 1);
        CHECK(b.level(sim, "CB2") == One);
        sim.runUntil(BusRise(3) + kMos6520Control);
        CHECK(b.level(sim, "CB2") == Zero);
        sim.runUntil(BusRise(5) + kMos6520Control - 1);
        CHECK(b.level(sim, "CB2") == Zero);
        sim.runUntil(BusRise(5) + kMos6520Control);
        CHECK(b.level(sim, "CB2") == One);
    }
    SECTION("pulse: low for one cycle") {
        const auto script = Script({BusWrite(kCrb, 0x2C), BusWrite(kRegB, 0x00)}, 4);
        BusBench b;
        b.build<Mos6520>(kPins, script);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusRise(3) + kMos6520Control);
        CHECK(b.level(sim, "CB2") == Zero);
        sim.runUntil(BusRise(4) + kMos6520Control - 1);
        CHECK(b.level(sim, "CB2") == Zero);
        sim.runUntil(BusRise(4) + kMos6520Control);
        CHECK(b.level(sim, "CB2") == One);
    }
    SECTION("a port B read does not strobe") {
        const auto script = Script({BusWrite(kCrb, 0x2C), BusRead(kRegB)}, 4);
        BusBench b;
        b.build<Mos6520>(kPins, script);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(BusFall(5));
        CHECK(b.level(sim, "CB2") == One);
    }
}

TEST_CASE("6520: C2 manual levels", "[parts]") {
    const auto script = Script({BusWrite(kCra, 0x34), BusWrite(kCrb, 0x34), BusIdle(), BusIdle(),
                                BusWrite(kCra, 0x3C), BusWrite(kCrb, 0x3C)});
    BusBench b;
    b.build<Mos6520>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(BusFall(4));
    CHECK(b.level(sim, "CA2") == Zero);
    CHECK(b.level(sim, "CB2") == Zero);
    sim.runUntil(BusFall(8));
    CHECK(b.level(sim, "CA2") == One);
    CHECK(b.level(sim, "CB2") == One);
}
