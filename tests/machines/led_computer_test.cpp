#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/ram2114.hpp"
#include "parts/rom_contents.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kPowerOnReset = 20_ms;
constexpr Time kStep = 124'780_us;  // one latch write to the next (docs: LED_COMPUTER_DESIGN)

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(test::NetlistPath("led_computer"));
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// Built from the routed Board (the Machine as the player gets it).
std::unique_ptr<LoadedCircuit> MachineCircuit() {
    auto machine = LoadNetlistMachine(test::NetlistPath("led_computer"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The reset vector, from the assembled ROM (so the test follows the program).
std::uint16_t ResetVector() {
    const auto words = ReadRomContents(test::RomPath("led_computer", "led_computer.rom"), 2048, 8);
    REQUIRE(words.has_value());
    return static_cast<std::uint16_t>((*words)[0x7FC] | (*words)[0x7FD] << 8);
}

NetId PartNet(const LoadedCircuit& loaded, const std::string& reference, const std::string& pin) {
    const Component& part = *loaded.components.at(reference);
    return loaded.circuit.netOf(PinId{ToIndex(part.firstPin()) + part.pinIndex(pin)});
}

// The LEDs' latch, the CPU's address and SYNC, RES_N and LATCH_CLK recorded from power-on.
struct Run {
    std::unique_ptr<LoadedCircuit> loaded;
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Run(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit = MachineCircuit())
        : loaded(std::move(circuit)) {
        for (int bit = 0; bit < 16; ++bit) {
            const std::string name = "A" + std::to_string(bit);
            nets[name] = PartNet(*loaded, "U1", name);
        }
        for (int bit = 0; bit < 8; ++bit) {
            nets["L" + std::to_string(bit)] = PartNet(*loaded, "U7", std::to_string(bit + 1) + "Q");
        }
        nets["PHI0"] = PartNet(*loaded, "U1", "PHI0");
        nets["SYNC"] = PartNet(*loaded, "U1", "SYNC");
        nets["RES_N"] = PartNet(*loaded, "U1", "RES_N");
        nets["LATCH_CLK"] = PartNet(*loaded, "U7", "CLK");
        sim.emplace(loaded->circuit);
        sim->powerOn(seed);
        for (const auto& [name, net] : nets) {
            sim->record(net);
        }
    }
    LogicValue at(const std::string& name, Time time) const {
        LogicValue value = X;
        for (const Sample& s : sim->recorder().samples(nets.at(name))) {
            if (s.time > time) {
                break;
            }
            value = s.value;
        }
        return value;
    }
    // The latch outputs as a byte; -1 if any is not 0 or 1.
    int leds(Time time) const {
        int value = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const LogicValue v = at("L" + std::to_string(bit), time);
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << bit;
        }
        return value;
    }
    std::uint16_t address(Time time) const {
        unsigned value = 0;
        for (int bit = 0; bit < 16; ++bit) {
            value |= (at("A" + std::to_string(bit), time) == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint16_t>(value);
    }
    // Times `name` went to `level` after `from`.
    std::vector<Time> edges(const std::string& name, LogicValue level, Time from) const {
        std::vector<Time> times;
        for (const Sample& s : sim->recorder().samples(nets.at(name))) {
            if (s.time > from && s.value == level) {
                times.push_back(s.time);
            }
        }
        return times;
    }
    // LATCH_CLK rises after the reset, which the 74LS02's random power-on outputs cannot fake.
    std::vector<Time> latchWrites(Time from) const { return edges("LATCH_CLK", One, from); }
};

bool Near(Time a, Time b, Time tolerance) { return a - b <= tolerance && b - a <= tolerance; }

}  // namespace

TEST_CASE("led_computer: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("led_computer: power-on reset", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    Run run(seed);
    run.sim->runUntil(kPowerOnReset + 1_ms);
    CHECK(run.at("RES_N", 0) == Zero);
    CHECK(run.at("RES_N", kPowerOnReset - 1) == Zero);
    CHECK(run.at("RES_N", kPowerOnReset) == One);
    CHECK(run.edges("RES_N", Zero, kPowerOnReset).empty());
    for (const Time t : {100_ns, 1_ms, 10_ms, kPowerOnReset - 1}) {
        CHECK(run.leds(t) == 0);  // cleared through CLR_N
    }
    // The first fetch after the reset sequence is at the reset vector.
    const std::uint16_t vector = ResetVector();
    std::optional<Time> fetch;
    for (const Time fall : run.edges("PHI0", Zero, kPowerOnReset)) {
        const Time t = fall + 500_ns;
        if (run.at("SYNC", t) == One) {
            fetch = t;
            break;
        }
    }
    REQUIRE(fetch.has_value());
    CHECK(run.address(*fetch) == vector);
    CHECK(*fetch < kPowerOnReset + 10_us);
    CHECK(run.leds(kPowerOnReset + 100_us) == 0x01);
}

TEST_CASE("led_computer: the light bounces", "[machines][slow]") {
    Run run(1);
    run.sim->runUntil(kPowerOnReset + 16 * kStep);
    const std::vector<Time> writes = run.latchWrites(kPowerOnReset);
    REQUIRE(writes.size() >= 16);
    constexpr std::array<int, 16> kLights{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                          0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x02};
    for (std::size_t i = 0; i < kLights.size(); ++i) {
        INFO("write " << i << " at " << writes[i]);
        CHECK(run.leds(writes[i] + 100_ns) == kLights[i]);
        if (i > 0) {
            CHECK(Near(writes[i] - writes[i - 1], kStep, 30_us));
        }
    }
}

TEST_CASE("led_computer: the variables live in RAM", "[machines]") {
    Run run(1);
    run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
    const std::vector<Time> writes = run.latchWrites(kPowerOnReset);
    REQUIRE(writes.size() == 3);
    REQUIRE(run.leds(writes[2] + 100_ns) == 0x04);
    const auto& low = static_cast<const Ram2114&>(*run.loaded->components.at("U3"));
    const auto& high = static_cast<const Ram2114&>(*run.loaded->components.at("U4"));
    const auto byte = [&](std::size_t address) {
        return low.word(address) | high.word(address) << 4;
    };
    CHECK(byte(0x000) == 0x04);                      // light
    CHECK(byte(0x001) == 0x00);                      // dir: moving left
    const int ret = byte(0x1FE) | byte(0x1FF) << 8;  // JSR's return address, minus one
    CHECK((ret & 0xF800) == 0xF800);
}

TEST_CASE("led_computer: Reset restarts the light", "[machines]") {
    Run run(1);
    Switch* button = SwitchOf(*run.loaded, "SW1");
    REQUIRE(button != nullptr);
    const Time press = kPowerOnReset + 2 * kStep + 10_ms;  // the light at 0x04
    run.sim->runUntil(press);
    REQUIRE(run.leds(press) == 0x04);
    button->setPressed(true);
    run.sim->wake(button->id());
    run.sim->runUntil(press + 1_ms);
    button->setPressed(false);
    run.sim->wake(button->id());
    run.sim->runUntil(press + 1_ms + kStep + 1_ms);
    CHECK(run.leds(press + 1_us) == 0);
    CHECK(run.leds(press + 1_ms) == 0);
    CHECK(run.leds(press + 1_ms + 100_us) == 0x01);
    CHECK(run.leds(press + 1_ms + kStep + 1_ms) == 0x02);
}

// Puzzles (LED_COMPUTER.md, Puzzles): each from power-on, faulty, then repaired.

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

std::unique_ptr<LoadedCircuit> Build(const Machine& machine) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The lights shown by the first `count` latch writes after the reset.
std::vector<int> FirstLights(std::unique_ptr<LoadedCircuit> circuit, std::uint64_t seed,
                             std::size_t count, Time until) {
    Run run(seed, std::move(circuit));
    run.sim->runUntil(until);
    std::vector<int> lights;
    for (const Time write : run.latchWrites(kPowerOnReset)) {
        if (lights.size() == count) {
            break;
        }
        lights.push_back(run.leds(write + 100_ns));
    }
    return lights;
}

const std::vector<int> kFirstThree{0x01, 0x02, 0x04};

// The fault-free start: the light at D1, D2, D3.
void CheckRepaired(const Machine& machine) {
    CHECK(FirstLights(Build(machine), 1, 3, kPowerOnReset + 2 * kStep + 1_ms) == kFirstThree);
}

// The LEDs at a few times over two steps.
void CheckShows(const Run& run, int lights) {
    for (const Time t : {kPowerOnReset + 50_ms, kPowerOnReset + kStep, kPowerOnReset + 2 * kStep}) {
        INFO("at " << t);
        CHECK(run.leds(t) == lights);
    }
}

}  // namespace

TEST_CASE("led_computer: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"lights_out", "sleeper", "full_house", "three_lights", "first_light",
                             "lightless", "blackout"}) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "led_computer");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("led_computer: lights_out", "[machines][slow]") {
    Machine machine = LoadPuzzle("lights_out");
    {
        Run run(1, Build(machine));
        const NetId strobe = PartNet(*run.loaded, "U6", "4Y");
        CHECK(strobe != run.nets.at("LATCH_CLK"));  // U7's clock pin cut off U6
        run.sim->record(strobe);
        run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
        int pulses = 0;  // the program still writes the LEDs once a step
        for (const Sample& s : run.sim->recorder().samples(strobe)) {
            pulses += s.time > kPowerOnReset && s.value == One ? 1 : 0;
        }
        CHECK(pulses == 3);
        CHECK(run.latchWrites(kPowerOnReset).empty());
        CheckShows(run, 0x00);
    }
    REQUIRE(machine.mend(LightsOutCrack().layer, LightsOutCrack().at));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: sleeper", "[machines][slow]") {
    Machine machine = LoadPuzzle("sleeper");
    {
        Run run(1, Build(machine));
        Switch* button = SwitchOf(*run.loaded, "SW1");
        REQUIRE(button != nullptr);
        run.sim->runUntil(kPowerOnReset + 50_ms);
        button->setPressed(true);  // Reset does not wake it
        run.sim->wake(button->id());
        run.sim->runUntil(kPowerOnReset + 60_ms);
        button->setPressed(false);
        run.sim->wake(button->id());
        run.sim->runUntil(kPowerOnReset + 2 * kStep);
        CHECK(run.edges("RES_N", One, 0).empty());
        CHECK(run.latchWrites(kPowerOnReset).empty());
        CheckShows(run, 0x00);
    }
    REQUIRE(machine.swap("U8"));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: full_house", "[machines][slow]") {
    Machine machine = LoadPuzzle("full_house");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        CHECK(*PinNet(*run.loaded, "U5", "Y0_N") == *PinNet(*run.loaded, "U5", "VCC"));
        run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
        CHECK_FALSE(run.latchWrites(kPowerOnReset).empty());
        CheckShows(run, 0xFF);
    }
    REQUIRE(machine.swap("U5"));  // the tempting wrong repair: the bridge stays
    {
        Run run(1, Build(machine));
        run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
        CheckShows(run, 0xFF);
    }
    REQUIRE(machine.removeBridge({"U5", "15"}, {"U5", "16"}));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: three_lights", "[machines][slow]") {
    Machine machine = LoadPuzzle("three_lights");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        CHECK(*PinNet(*run.loaded, "U6", "3A") == *PinNet(*run.loaded, "U6", "3B"));
        run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
        CheckShows(run, 0x13);  // JSR's return address low byte, the last write before the delay
    }
    REQUIRE(machine.swap("U6"));  // the tempting wrong repair: the bridge stays
    {
        Run run(1, Build(machine));
        run.sim->runUntil(kPowerOnReset + 2 * kStep + 1_ms);
        CheckShows(run, 0x13);
    }
    REQUIRE(machine.removeBridge({"U6", "8"}, {"U6", "9"}));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: first_light", "[machines][slow]") {
    // Bit 7 of the high byte JSR pushes at $01FF (U4 holds DB4-DB7): RTS goes to $7814, where
    // nothing answers; the bus reads $FF, which the 6502 runs up to $E000, the ROM's mirror of
    // $F800, and the program starts again: D1 lights again and again.
    Machine machine = LoadPuzzle("first_light");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        run.sim->runUntil(kPowerOnReset + 2 * kStep);
        const std::vector<Time> writes = run.latchWrites(kPowerOnReset);
        REQUIRE_FALSE(writes.empty());
        for (const Time write : writes) {
            CHECK(run.leds(write + 100_ns) == 0x01);
        }
    }
    REQUIRE(machine.swap("U4"));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: lightless", "[machines][slow]") {
    // U3 reads A0 as 0: light ($00) and dir ($01) share their low nibble, so `sta dir` clears
    // the light; JSR's two return bytes share word $1FE, so RTS goes to erased ROM ($FF), which
    // runs on to $F800 and starts the program again.
    Machine machine = LoadPuzzle("lightless");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        // Each restart adds a few ms to the step: run a step longer.
        CHECK(FirstLights(Build(machine), seed, 3, kPowerOnReset + 3 * kStep) ==
              std::vector<int>{0x00, 0x00, 0x00});
    }
    REQUIRE(machine.swap("U3"));
    CheckRepaired(machine);
}

TEST_CASE("led_computer: blackout", "[machines][slow]") {
    // The CPU runs the open bus and the RAM's random contents: seed 1 never writes the LEDs,
    // seed 8 writes them once (at about 146 ms) and freezes there. No light ever runs.
    Machine machine = LoadPuzzle("blackout");
    for (const auto& [seed, lights] : {std::pair<std::uint64_t, int>{1, 0x00}, {8, 0x46}}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        run.sim->runUntil(kPowerOnReset + 3 * kStep);
        CHECK_FALSE(run.edges("A15", One, kPowerOnReset).empty());  // the CPU runs
        for (const Time t : {kPowerOnReset + 2 * kStep, kPowerOnReset + 3 * kStep}) {
            INFO("at " << t);
            CHECK(run.leds(t) == lights);
        }
    }
    REQUIRE(machine.swap("U2"));
    CheckRepaired(machine);
}
