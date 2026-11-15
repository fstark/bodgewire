// The 6522 Part on a scripted 1 MHz bus: pins, bus and port timing, the outputs from its core
// (P5n). The logic itself is tested cycle by cycle in mos6522_core_test.cpp.

#include "parts/mos6522.hpp"

#include <array>
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

constexpr unsigned kOrb = 0, kDdrb = 2, kDdra = 3, kOra = 1, kT1L = 4, kT1H = 5, kSr = 0xA,
                   kAcr = 0xB, kIfr = 0xD, kIer = 0xE;

const BusPins kPins{Mos6522::kPhi2,
                    Mos6522::kRw,
                    Mos6522::kResN,
                    Mos6522::kData,
                    {Mos6522::kRs[0], Mos6522::kRs[1], Mos6522::kRs[2], Mos6522::kRs[3]},
                    {{Mos6522::kCs1, true}, {Mos6522::kCs2N, false}}};

BusCycle Reset() { return {0, true, 0, false, true}; }

std::vector<BusCycle> Script(std::vector<BusCycle> script, std::size_t idle = 4) {
    script.insert(script.begin(), Reset());
    script.insert(script.end(), idle, BusIdle());
    return script;
}

}  // namespace

TEST_CASE("6522: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("6522");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-40");
    const auto pins = Mos6522::Pins();
    REQUIRE(pins.size() == 40);
    const std::array<std::string_view, 40> names{
        "GND",   "PA0", "PA1",   "PA2",   "PA3",  "PA4", "PA5", "PA6", "PA7", "PB0",
        "PB1",   "PB2", "PB3",   "PB4",   "PB5",  "PB6", "PB7", "CB1", "CB2", "VCC",
        "IRQ_N", "RW",  "CS2_N", "CS1",   "PHI2", "D7",  "D6",  "D5",  "D4",  "D3",
        "D2",    "D1",  "D0",    "RES_N", "RS3",  "RS2", "RS1", "RS0", "CA2", "CA1"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(names[i]);
        CHECK(pins[i].name == names[i]);
        if (names[i] == "GND" || names[i] == "VCC") {
            CHECK(pins[i].kind == PinKind::Power);
            continue;
        }
        CHECK(pins[i].family == Family::Mos);
        CHECK(pins[i].sampled == (names[i] != "PHI2" && names[i] != "IRQ_N"));
        PinKind kind = PinKind::Input;
        if (names[i].starts_with("PA") || names[i].starts_with("PB") || names[i] == "CA2" ||
            names[i] == "CB1" || names[i] == "CB2") {
            kind = PinKind::PullUp;
        } else if (names[i].starts_with("D")) {
            kind = PinKind::Bidirectional;
        } else if (names[i] == "IRQ_N") {
            kind = PinKind::OpenCollector;
        }
        CHECK(pins[i].kind == kind);
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        CHECK(pins[Mos6522::kData[bit]].name == "D" + std::to_string(bit));
        CHECK(pins[Mos6522::kPortA[bit]].name == "PA" + std::to_string(bit));
        CHECK(pins[Mos6522::kPortB[bit]].name == "PB" + std::to_string(bit));
    }
    for (unsigned bit = 0; bit < 4; ++bit) {
        CHECK(pins[Mos6522::kRs[bit]].name == "RS" + std::to_string(bit));
    }
}

TEST_CASE("6522: bus and port delays", "[parts]") {
    // PA0 an output at 1 (cycles 1-2); ORA = 0 in cycle 5; DDRA read in cycle 7.
    const auto script = Script({BusWrite(kDdra, 0x01), BusWrite(kOra, 0x01), BusIdle(), BusIdle(),
                                BusWrite(kOra, 0x00), BusIdle(), BusRead(kDdra)});
    BusBench b;
    b.build<Mos6522>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(BusFall(5) + kMos6522Port - 1);
    CHECK(b.level(sim, "PA0") == One);
    sim.runUntil(BusFall(5) + kMos6522Port);
    CHECK(b.level(sim, "PA0") == Zero);
    CHECK(b.level(sim, "PA1") == One);  // an input: the pull-up
    sim.runUntil(BusRise(7) + kMos6522DataOut - 1);
    CHECK_FALSE(b.bus(sim).has_value());
    sim.runUntil(BusRise(7) + kMos6522DataOut);
    CHECK(b.bus(sim) == 0x01u);
    sim.runUntil(BusFall(7) + kMos6522DataHold);
    CHECK_FALSE(b.bus(sim).has_value());
}

TEST_CASE("6522: a port read gives the pins on inputs, the register on PB outputs", "[parts]") {
    BusOutside outside{{"PA3", {Zero, {}}}, {"PB0", {Zero, {}}}, {"PB1", {Zero, {}}}};
    const auto script = Script({BusWrite(kDdrb, 0x01), BusWrite(kOrb, 0x01), BusIdle(), BusIdle(),
                                BusRead(kOra), BusRead(kOrb)});
    BusBench b;
    b.build<Mos6522>(kPins, script, outside);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 2);
    CHECK(reads[0] == 0xF7u);
    CHECK(reads[1] == 0xFDu);  // PB0 the register's 1, PB1 the pin's 0
}

TEST_CASE("6522: T1 on PB7 and IRQ_N", "[parts]") {
    // Free-running with PB7 (ACR $C0), T1 IRQ on; N = 3: PB7 low from the write's fall, toggling
    // every 5 cycles from the fall of the 0 cycle (write + 4).
    const auto script = Script(
        {BusWrite(kAcr, 0xC0), BusWrite(kIer, 0xC0), BusWrite(kT1L, 3), BusWrite(kT1H, 0)}, 20);
    BusBench b;
    b.build<Mos6522>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    const std::size_t w = 4;  // the T1H write's cycle
    sim.runUntil(BusFall(w) + kMos6522Shift);
    CHECK(b.level(sim, "PB7") == Zero);
    sim.runUntil(BusFall(w + 4) + kMos6522Irq - 1);
    CHECK(b.level(sim, "IRQ_N") != Zero);
    sim.runUntil(BusFall(w + 4) + kMos6522Irq);
    CHECK(b.level(sim, "IRQ_N") == Zero);
    sim.runUntil(BusFall(w + 4) + kMos6522Shift - 1);
    CHECK(b.level(sim, "PB7") == Zero);
    sim.runUntil(BusFall(w + 4) + kMos6522Shift);
    CHECK(b.level(sim, "PB7") == One);
    sim.runUntil(BusFall(w + 9) + kMos6522Shift);
    CHECK(b.level(sim, "PB7") == Zero);
}

TEST_CASE("6522: shift register drives CB1 and CB2 in its output modes only", "[parts]") {
    // Mode 6 (out under φ2) from cycle 1, SR = $80 in cycle 2: CB1 a clock, CB2 the bits; then
    // mode 0 in cycle 25: both back to their pull-ups.
    std::vector<BusCycle> body{BusWrite(kAcr, 0x18), BusWrite(kSr, 0x80)};
    body.insert(body.end(), 22, BusIdle());
    body.push_back(BusWrite(kAcr, 0x00));
    const auto script = Script(body);
    BusBench b;
    b.build<Mos6522>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(BusFall(3) + kMos6522Shift);
    CHECK(b.level(sim, "CB1") == Zero);
    CHECK(b.level(sim, "CB2") == One);  // bit 7
    sim.runUntil(BusFall(4) + kMos6522Shift);
    CHECK(b.level(sim, "CB1") == One);
    sim.runUntil(BusFall(5) + kMos6522Shift);
    CHECK(b.level(sim, "CB2") == Zero);  // bit 6
    sim.runUntil(BusFall(30));
    CHECK(b.level(sim, "CB1") == One);
    CHECK(b.level(sim, "CB2") == One);
}

TEST_CASE("6522: reset clears the registers and releases the outputs", "[parts]") {
    const auto script = Script({BusWrite(kDdra, 0xFF), BusWrite(kOra, 0x00), BusWrite(kIer, 0xFF),
                                BusIdle(), BusIdle(), Reset(), BusIdle(), BusIdle(), BusRead(kDdra),
                                BusRead(kIer), BusRead(kIfr)});
    BusBench b;
    b.build<Mos6522>(kPins, script);
    Simulator sim(b.circuit);
    sim.powerOn(2);
    sim.runUntil(BusFall(5));
    CHECK(b.level(sim, "PA0") == Zero);
    const auto reads = RunReads(b, sim, script);
    REQUIRE(reads.size() == 3);
    CHECK(reads[0] == 0x00u);
    CHECK(reads[1] == 0x80u);
    CHECK(reads[2] == 0x00u);
    CHECK(b.level(sim, "PA0") == One);
    CHECK(b.level(sim, "IRQ_N") == Z);
}
