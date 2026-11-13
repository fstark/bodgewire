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

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/button.hpp"
#include "parts/ram2114.hpp"
#include "parts/rom_contents.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kPowerOnReset = 20_ms;
constexpr Time kStep = 124'780_us;  // one latch write to the next (docs: LED_COMPUTER_DESIGN)

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "led_computer.bwn");
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
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "led_computer.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The reset vector, from the assembled ROM (so the test follows the program).
std::uint16_t ResetVector() {
    const auto words =
        ReadRomContents(std::filesystem::path(BW_ROM_DIR) / "led_computer.rom", 2048, 8);
    REQUIRE(words.has_value());
    return static_cast<std::uint16_t>((*words)[0x7FC] | (*words)[0x7FD] << 8);
}

NetId PartNet(const LoadedCircuit& loaded, const std::string& reference, const std::string& pin) {
    const Component& part = *loaded.components.at(reference);
    return loaded.circuit.netOf(PinId{ToIndex(part.firstPin()) + part.pinIndex(pin)});
}

// The LEDs' latch, the CPU's address and SYNC, RES_N and LATCH_CLK recorded from power-on.
struct Run {
    std::unique_ptr<LoadedCircuit> loaded = MachineCircuit();
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Run(std::uint64_t seed) {
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
    Button* button = ButtonOf(*run.loaded, "SW1");
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
