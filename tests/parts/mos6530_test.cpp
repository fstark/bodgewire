#include "parts/mos6530.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using namespace delays;

// One bus cycle of 1 us: PHI2 low for the first half, high for the second (rise at +500 ns, fall
// at the cycle's end). Address and controls change 100 ns into the cycle; a write's data is on
// the bus from 150 ns after the rise to 30 ns after the fall.
struct Cycle {
    unsigned address = 0;  // A9..A0
    bool read = true;
    std::uint8_t data = 0;  // written
    bool rs0 = true;        // high: not the ROM
    bool cs1 = false;       // low: RAM and I/O selectable
    bool reset = false;     // RES_N low
    bool idle = false;      // not a read RunReads reports
};

constexpr unsigned kIo002 = 0x340;   // A9-A6 1101
constexpr unsigned kRam002 = 0x3C0;  // A9-A6 1111
constexpr unsigned kIo003 = 0x300;   // A9-A6 1100
constexpr unsigned kRam003 = 0x380;  // A9-A6 1110

Cycle Read(unsigned address) { return {address, true}; }
Cycle Write(unsigned address, std::uint8_t data) { return {address, false, data}; }
Cycle Idle() { return {0, true, 0, true, true, false, true}; }  // nothing selected

Time Rise(std::size_t cycle) { return static_cast<Time>(cycle) * 1_us + 500_ns; }
Time Fall(std::size_t cycle) { return static_cast<Time>(cycle + 1) * 1_us; }

std::vector<std::uint8_t> RomPattern() {
    std::vector<std::uint8_t> words(1024);
    for (std::size_t a = 0; a < words.size(); ++a) {
        words[a] = static_cast<std::uint8_t>(a * 7 + 3);
    }
    return words;
}

using Steps = std::vector<Stimulus::Step>;

// Steps for a level that is `at(i)` from `start(i)` in each cycle i.
template <typename At, typename Start>
Steps Levels(std::size_t cycles, LogicValue& initial, At at, Start start) {
    Steps steps;
    initial = at(0);
    LogicValue last = initial;
    for (std::size_t i = 1; i < cycles; ++i) {
        const LogicValue v = at(i);
        if (v != last) {
            steps.push_back({start(i), v});
            last = v;
        }
    }
    return steps;
}

// One 6530 driven through a script of cycles; each port Pin on a Net of its own, with an
// optional outside Driver (MOS, Strong; Z unless listed).
struct Bench {
    Circuit circuit;
    Mos6530* chip = nullptr;
    std::array<NetId, 8> data{};
    std::map<std::string, NetId> ports;

    Bench(const std::vector<Cycle>& script, std::string_view mask = "002",
          const std::map<std::string, std::pair<LogicValue, Steps>>& outside = {}) {
        chip = &circuit.add<Mos6530>("U1", *FindMos6530Mask(mask), RomPattern());
        const std::size_t n = script.size();
        const auto input = [&](PinIndex pin, LogicValue initial, Steps steps) {
            const NetId net = circuit.addNet(std::string(chip->pins()[pin].name));
            auto& source = circuit.add<Stimulus>("S" + std::string(chip->pins()[pin].name), initial,
                                                 std::move(steps));
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(*chip, pin, net);
        };
        const auto control = [&](PinIndex pin, auto value) {
            LogicValue initial = Zero;
            Steps steps = Levels(
                n, initial, [&](std::size_t i) { return value(script[i]) ? One : Zero; },
                [](std::size_t i) { return static_cast<Time>(i) * 1_us + 100_ns; });
            input(pin, initial, std::move(steps));
        };
        Steps phi2;
        for (std::size_t i = 0; i < n; ++i) {
            phi2.push_back({Rise(i), One});
            phi2.push_back({Fall(i), Zero});
        }
        input(Mos6530::kPhi2, Zero, phi2);
        control(Mos6530::kRs0, [](const Cycle& c) { return c.rs0; });
        control(Mos6530::kCs1, [](const Cycle& c) { return c.cs1; });
        control(Mos6530::kRw, [](const Cycle& c) { return c.read; });
        control(Mos6530::kResN, [](const Cycle& c) { return !c.reset; });
        for (unsigned bit = 0; bit < 10; ++bit) {
            control(Mos6530::kAddress[bit],
                    [bit](const Cycle& c) { return ((c.address >> bit) & 1) != 0; });
        }
        for (unsigned bit = 0; bit < 8; ++bit) {
            Steps steps;
            for (std::size_t i = 0; i < n; ++i) {
                if (!script[i].read) {
                    steps.push_back({Rise(i) + 150_ns, ((script[i].data >> bit) & 1) ? One : Zero});
                    steps.push_back({Fall(i) + 30_ns, Z});
                }
            }
            data[bit] = circuit.addNet("D" + std::to_string(bit));
            auto& source =
                circuit.add<Stimulus>("SD" + std::to_string(bit), Z, std::move(steps), Family::Mos);
            circuit.connect(source, Stimulus::kOut, data[bit]);
            circuit.connect(*chip, Mos6530::kData[bit], data[bit]);
        }
        const auto port = [&](PinIndex pin) {
            const std::string name(chip->pins()[pin].name);
            const NetId net = circuit.addNet(name);
            ports[name] = net;
            circuit.connect(*chip, pin, net);
            if (const auto it = outside.find(name); it != outside.end()) {
                auto& source = circuit.add<Stimulus>("S" + name, it->second.first,
                                                     it->second.second, Family::Mos);
                circuit.connect(source, Stimulus::kOut, net);
            }
        };
        for (const PinIndex pin : Mos6530::kPortA) {
            port(pin);
        }
        for (const PinIndex pin : Mos6530::kPortB) {
            if (pin != Mos6530::kNoPin) {
                port(pin);
            }
        }
        circuit.finalize();
    }

    // The data bus: its byte, or nullopt if any line floats or is X.
    std::optional<unsigned> bus(const Simulator& sim) const {
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const LogicValue v = sim.value(data[bit]);
            if (sim.isFloating(data[bit]) || (v != Zero && v != One)) {
                return std::nullopt;
            }
            value |= (v == One ? 1u : 0u) << bit;
        }
        return value;
    }

    LogicValue port(const Simulator& sim, const std::string& name) const {
        const NetId net = ports.at(name);
        return sim.isFloating(net) ? Z : sim.value(net);
    }
};

// Runs the script; the bus just before each read cycle's PHI2 fall (nullopt: floating).
std::vector<std::optional<unsigned>> RunReads(Bench& bench, Simulator& sim,
                                              const std::vector<Cycle>& script) {
    std::vector<std::optional<unsigned>> reads;
    for (std::size_t i = 0; i < script.size(); ++i) {
        if (Fall(i) - 1 > sim.now()) {
            sim.runUntil(Fall(i) - 1);
        }
        if (script[i].read && !script[i].idle) {
            reads.push_back(bench.bus(sim));
        }
    }
    return reads;
}

}  // namespace

TEST_CASE("6530: catalogue row, pins and parameters", "[parts]") {
    const PartInfo* part = FindPart("6530");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-40");
    CHECK(part->kind == PartKind::Chip);

    const auto pins = Mos6530::Pins();
    REQUIRE(pins.size() == 40);
    const std::array<std::string_view, 40> names{
        "GND", "PA0", "PHI2", "RS0", "A9",  "A8",    "A7",  "A6",  "RW",  "A5",
        "A4",  "A3",  "A2",   "A1",  "A0",  "RES_N", "PB7", "CS1", "PB5", "VCC",
        "PB4", "PB3", "PB2",  "PB1", "PB0", "D7",    "D6",  "D5",  "D4",  "D3",
        "D2",  "D1",  "D0",   "PA7", "PA6", "PA5",   "PA4", "PA3", "PA2", "PA1"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
        if (names[i] == "GND" || names[i] == "VCC") {
            CHECK(pins[i].kind == PinKind::Power);
            continue;
        }
        CHECK(pins[i].family == Family::Mos);
        CHECK(pins[i].sampled == (names[i] != "PHI2"));
        const bool port = names[i].starts_with("PA") || names[i].starts_with("PB");
        CHECK(pins[i].kind == (port                        ? PinKind::PullUp
                               : names[i].starts_with("D") ? PinKind::Bidirectional
                                                           : PinKind::Input));
    }
    for (unsigned bit = 0; bit < 10; ++bit) {
        CHECK(pins[Mos6530::kAddress[bit]].name == "A" + std::to_string(bit));
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        CHECK(pins[Mos6530::kData[bit]].name == "D" + std::to_string(bit));
        CHECK(pins[Mos6530::kPortA[bit]].name == "PA" + std::to_string(bit));
        if (bit != 6) {
            CHECK(pins[Mos6530::kPortB[bit]].name == "PB" + std::to_string(bit));
        }
    }
    CHECK(Mos6530::kPortB[6] == Mos6530::kNoPin);

    const std::string rom = std::string(BW_ROOT_DIR) + "/tests/data/empty.rom";
    const auto create = [&](std::vector<PartParam> params) {
        Circuit circuit;
        const auto created = part->create(circuit, "U1", params);
        return created ? std::string() : created.error();
    };
    CHECK(create({{"mask", "002"}, {"contents", rom}}).empty());
    CHECK(create({{"mask", "003"}, {"contents", rom}}).empty());
    CHECK(create({{"contents", rom}}) == "6530 needs mask=002|003");
    CHECK(create({{"mask", "004"}, {"contents", rom}}) == "6530: unknown mask '004' (002|003)");
    CHECK(create({{"mask", "002"}}) == "6530 needs contents=<file>");
    CHECK(create({{"mask", "002"}, {"contents", rom}, {"freq", "1MHz"}}) ==
          "6530: unknown parameter 'freq'");
}

TEST_CASE("6530: ROM reads, their delay and release", "[parts]") {
    // Cycle 1: ROM $005 (RS0 low, any CS1); cycle 3: ROM $3FF with CS1 high.
    std::vector<Cycle> script{
        Idle(), {0x005, true, 0, false, false}, Idle(), {0x3FF, true, 0, false, true}, Idle()};
    Bench bench(script);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(Rise(1) + kMos6530DataOut - 1);
    CHECK_FALSE(bench.bus(sim).has_value());
    sim.runUntil(Rise(1) + kMos6530DataOut);
    CHECK(bench.bus(sim) == (5u * 7 + 3) % 256);
    sim.runUntil(Fall(1) + kMos6530DataHold - 1);
    CHECK(bench.bus(sim).has_value());
    sim.runUntil(Fall(1) + kMos6530DataHold);
    CHECK_FALSE(bench.bus(sim).has_value());
    sim.runUntil(Fall(3) - 1);
    CHECK(bench.bus(sim) == (0x3FFu * 7 + 3) % 256);
}

TEST_CASE("6530: RAM and I/O at each mask's addresses only", "[parts]") {
    SECTION("002") {
        const std::vector<Cycle> script{
            Write(kRam002 | 0x05, 0xA5),
            Read(kRam002 | 0x05),
            Write(kRam002 | 0x3F, 0x5A),
            Read(kRam002 | 0x3F),
            Read(kRam003 | 0x05),                     // the 003's RAM: nobody
            Read(kIo003 | 0x01),                      // the 003's I/O: nobody
            {kRam002 | 0x05, true, 0, true, true},    // CS1 high: nobody
            {kRam002 | 0x05, true, 0, false, false},  // RS0 low: the ROM
            Write(kIo002 | 0x01, 0x3C),
            Read(kIo002 | 0x01)};
        Bench bench(script);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        const auto reads = RunReads(bench, sim, script);
        CHECK(reads[0] == 0xA5u);
        CHECK(reads[1] == 0x5Au);
        CHECK_FALSE(reads[2].has_value());
        CHECK_FALSE(reads[3].has_value());
        CHECK_FALSE(reads[4].has_value());
        CHECK(reads[5] == ((kRam002 | 0x05) * 7 + 3) % 256);
        CHECK(reads[6] == 0x3Cu);
        CHECK(bench.chip->ram(0x05) == 0xA5);
    }
    SECTION("003") {
        const std::vector<Cycle> script{Write(kRam003 | 0x07, 0x81), Read(kRam003 | 0x07),
                                        Read(kRam002 | 0x07),        Write(kIo003 | 0x03, 0x42),
                                        Read(kIo003 | 0x03),         Read(kIo002 | 0x03)};
        Bench bench(script, "003");
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        const auto reads = RunReads(bench, sim, script);
        CHECK(reads[0] == 0x81u);
        CHECK_FALSE(reads[1].has_value());
        CHECK(reads[2] == 0x42u);
        CHECK_FALSE(reads[3].has_value());
    }
}

TEST_CASE("6530: ports, pull-ups and outside drives", "[parts]") {
    // Direction A $0F, data A $05: PA0 1, PA1 0, PA2 1, PA3 0; PA4-PA7 inputs. Outside: PA0 pulled
    // low from 10 us, PA4 low from 10 us.
    const std::vector<Cycle> script{Write(kIo002 | 0x01, 0x0F),
                                    Write(kIo002 | 0x00, 0x05),
                                    Idle(),
                                    Idle(),
                                    Read(kIo002 | 0x00),
                                    Idle(),
                                    Idle(),
                                    Idle(),
                                    Idle(),
                                    Idle(),
                                    Idle(),
                                    Read(kIo002 | 0x00),
                                    Read(kIo002 | 0x01)};
    Bench bench(script, "002", {{"PA0", {Z, {{10_us, Zero}}}}, {"PA4", {Z, {{10_us, Zero}}}}});
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    // Data A written at the fall of cycle 1 (2 us): the Pins kMos6530Port later.
    sim.runUntil(Fall(1) + kMos6530Port);
    CHECK(bench.port(sim, "PA0") == One);
    CHECK(bench.port(sim, "PA1") == Zero);
    CHECK(bench.port(sim, "PA2") == One);
    CHECK(bench.port(sim, "PA3") == Zero);
    for (const char* input : {"PA4", "PA5", "PA6", "PA7"}) {
        CHECK(bench.port(sim, input) == One);  // the pull-up alone: 1, not floating
    }
    const auto reads = RunReads(bench, sim, script);
    CHECK(reads[0] == 0xF5u);  // inputs read 1 from their pull-ups
    // PA0 an output at 1, pulled low outside: the Net is 0, the read gives the register's 1.
    CHECK(bench.port(sim, "PA0") == Zero);
    CHECK(bench.port(sim, "PA4") == Zero);
    CHECK(reads[1] == 0xE5u);  // PA4 reads its Pin: 0
    CHECK(reads[2] == 0x0Fu);
}

TEST_CASE("6530: a port output changes kMos6530Port after the write", "[parts]") {
    const std::vector<Cycle> script{Write(kIo002 | 0x03, 0xFF), Write(kIo002 | 0x02, 0x00),
                                    Write(kIo002 | 0x02, 0x21), Idle(), Idle()};
    Bench bench(script);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(Fall(1) + kMos6530Port);
    CHECK(bench.port(sim, "PB0") == Zero);
    CHECK(bench.port(sim, "PB5") == Zero);
    sim.runUntil(Fall(2) + kMos6530Port - 1);
    CHECK(bench.port(sim, "PB0") == Zero);
    sim.runUntil(Fall(2) + kMos6530Port);
    CHECK(bench.port(sim, "PB0") == One);
    CHECK(bench.port(sim, "PB1") == Zero);
    CHECK(bench.port(sim, "PB5") == One);
}

TEST_CASE("6530: RES_N clears the ports and floats the bus", "[parts]") {
    std::vector<Cycle> script{Write(kIo002 | 0x01, 0xFF), Write(kIo002 | 0x00, 0x00), Idle(),
                              Idle()};
    Cycle reset = Read(kIo002 | 0x01);
    reset.reset = true;
    script.push_back(reset);
    script.push_back(Idle());
    script.push_back(Read(kIo002 | 0x01));
    script.push_back(Read(kIo002 | 0x00));
    Bench bench(script);
    Simulator sim(bench.circuit);
    sim.powerOn(3);
    sim.runUntil(Fall(3));
    CHECK(bench.port(sim, "PA3") == Zero);
    const auto reads = RunReads(bench, sim, script);
    CHECK_FALSE(reads[0].has_value());  // the read during reset
    CHECK(reads[1] == 0x00u);           // direction cleared
    CHECK(reads[2] == 0xFFu);           // every Pin an input, pulled up
    CHECK(bench.port(sim, "PA3") == One);
}

TEST_CASE("6530: the timer counts as the data sheet's Fig. 5", "[parts]") {
    // N = 52, ÷8, written in cycle 0 (pulse 0 of the figure); reads k pulses later.
    std::vector<Cycle> script(501, Idle());
    script[0] = Write(kIo002 | 0x05, 52);  // A2 = 1, A1 A0 = 01, A3 = 0
    for (const std::size_t k : {1, 7, 8, 213, 415, 416, 417, 500}) {
        script[k] = Read(kIo002 | 0x04);
    }
    Bench bench(script);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(bench, sim, script);
    REQUIRE(reads.size() == 8);
    CHECK(reads[0] == 51u);
    CHECK(reads[1] == 51u);
    CHECK(reads[2] == 50u);
    CHECK(reads[3] == 25u);
    CHECK(reads[4] == 0u);
    CHECK(reads[5] == 0xFFu);
    CHECK(reads[6] == 0xFEu);
    CHECK(reads[7] == 0xABu);  // 84 pulses after $FF
}

TEST_CASE("6530: the timer's other rates and N = 0", "[parts]") {
    const auto read = [](unsigned rate, std::uint8_t n, std::size_t k) {
        std::vector<Cycle> script(k + 1, Idle());
        script[0] = Write(kIo002 | 0x04 | rate, n);
        script[k] = Read(kIo002 | 0x04);
        Bench bench(script);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        return RunReads(bench, sim, script).at(0);
    };
    CHECK(read(0, 3, 1) == 1u);  // ÷1: 3 - 1 - 1
    CHECK(read(0, 3, 2) == 0u);
    CHECK(read(0, 3, 3) == 0xFFu);
    CHECK(read(2, 3, 63) == 2u);  // ÷64
    CHECK(read(2, 3, 64) == 1u);
    CHECK(read(2, 3, 192) == 0xFFu);
    CHECK(read(3, 1, 1023) == 0u);  // ÷1024
    CHECK(read(3, 1, 1024) == 0xFFu);
    CHECK(read(1, 0, 1) == 0xFEu);  // N = 0: the flag at once, $FF at the write's pulse
}

TEST_CASE("6530: the interrupt on PB7", "[parts]") {
    SECTION("enabled: low before pulse N·P, held by a read in that pulse, cleared after") {
        std::vector<Cycle> script(420, Idle());
        script[0] = Write(kIo002 | 0x0D, 52);  // ÷8, A3 = 1
        script[100] = Read(kIo002 | 0x05);     // the flag
        script[416] = Read(kIo002 | 0x0C);     // the timer, A3 = 1, in the flag's pulse
        script[417] = Read(kIo002 | 0x0D);     // the flag
        script[418] = Read(kIo002 | 0x0C);     // the timer, after: clears
        Bench bench(script);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        sim.runUntil(Fall(415) + kMos6530Irq - 1);
        CHECK(bench.port(sim, "PB7") == One);
        sim.runUntil(Fall(415) + kMos6530Irq);
        CHECK(bench.port(sim, "PB7") == Zero);
        CHECK(Rise(416) - (Fall(415) + kMos6530Irq) >= 200_ns);  // TIC
        sim.runUntil(Fall(417));
        CHECK(bench.port(sim, "PB7") == Zero);
        sim.runUntil(Rise(418) + kMos6530Irq - 1);
        CHECK(bench.port(sim, "PB7") == Zero);
        sim.runUntil(Rise(418) + kMos6530Irq);
        CHECK(bench.port(sim, "PB7") == One);
        Bench second(script);
        Simulator again(second.circuit);
        again.powerOn(1);
        const auto reads = RunReads(second, again, script);
        CHECK(reads[0] == 0x00u);  // no flag yet
        CHECK(reads[1] == 0xFFu);  // the timer in the flag's pulse
        CHECK(reads[2] == 0x80u);  // the flag, not cleared by that read
        CHECK(reads[3] == 0xFDu);
    }
    SECTION("disabled: PB7 stays high, the flag still sets") {
        std::vector<Cycle> script(420, Idle());
        script[0] = Write(kIo002 | 0x05, 52);  // ÷8, A3 = 0
        script[418] = Read(kIo002 | 0x05);
        Bench bench(script);
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        sim.runUntil(Fall(417));
        CHECK(bench.port(sim, "PB7") == One);
        Bench second(script);
        Simulator again(second.circuit);
        again.powerOn(1);
        CHECK(RunReads(second, again, script).at(0) == 0x80u);
    }
}

TEST_CASE("6530: stuck cells in the ROM and the RAM", "[parts]") {
    const std::vector<Cycle> script{
        {0x005, true, 0, false, false}, Write(kRam002 | 0x03, 0x00), Read(kRam002 | 0x03)};
    Bench bench(script);
    REQUIRE(bench.chip->memories().size() == 2);
    CHECK(bench.chip->memories()[0] == "rom");
    CHECK(bench.chip->memories()[1] == "ram");
    CHECK(bench.chip->wordCount(0) == 1024);
    CHECK(bench.chip->wordCount(1) == 64);
    CHECK(bench.chip->dataBit(Mos6530::kData[3]) == 3u);
    CHECK_FALSE(bench.chip->dataBit(Mos6530::kPortA[3]).has_value());
    bench.chip->stick(0, 5, 0, true);        // ROM $005 = $26: bit 0 set → $27
    bench.chip->stick(1, 3, 7, true);        // RAM $03 bit 7
    CHECK(bench.chip->read(0, 5) == 0x27u);  // P6p: read through the stuck cell
    CHECK(bench.chip->wordBits(0) == 8);
    CHECK(bench.chip->wordBits(1) == 8);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    const auto reads = RunReads(bench, sim, script);
    CHECK(reads[0] == 0x27u);
    CHECK(reads[1] == 0x80u);
}
