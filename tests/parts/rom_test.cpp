#include "parts/rom.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
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

struct Input {
    LogicValue initial;
    std::vector<Stimulus::Step> steps;
};

// Word = address x 7, masked to the data width.
std::vector<std::uint8_t> Pattern(const RomSpec& spec) {
    std::vector<std::uint8_t> words(std::size_t{1} << spec.address.size());
    const unsigned mask = (1u << spec.data.size()) - 1;
    for (std::size_t a = 0; a < words.size(); ++a) {
        words[a] = static_cast<std::uint8_t>((a * 7) & mask);
    }
    return words;
}

// One ROM with the pattern; every address and enable Pin driven (listed or 0), data on Nets.
struct Bench {
    Circuit circuit;
    std::map<std::string, NetId, std::less<>> nets;
    const RomSpec& spec;

    Bench(std::string_view part, std::map<std::string, Input> inputs) : spec(RomSpecOf(part)) {
        auto& chip = circuit.add<Rom>("U1", spec, Pattern(spec));
        const auto drive = [&](PinIndex pin) {
            const std::string name(spec.names[pin]);
            Input input{Zero, {}};
            if (const auto it = inputs.find(name); it != inputs.end()) {
                input = it->second;
            }
            const NetId net = circuit.addNet(name);
            auto& source = circuit.add<Stimulus>("S" + name, input.initial, input.steps);
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(chip, pin, net);
            nets[name] = net;
        };
        for (const PinIndex pin : spec.address) {
            drive(pin);
        }
        for (const RomEnable& enable : spec.enables) {
            drive(enable.pin);
        }
        for (const PinIndex pin : spec.data) {
            const std::string name(spec.names[pin]);
            nets[name] = circuit.addNet(name);
            circuit.connect(chip, pin, nets[name]);
        }
        circuit.finalize();
    }

    // The data Pins as a number; -1 if any is not 0 or 1.
    int word(const Simulator& sim) const {
        int value = 0;
        for (std::size_t b = 0; b < spec.data.size(); ++b) {
            const LogicValue v = sim.value(nets.find(spec.names[spec.data[b]])->second);
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << b;
        }
        return value;
    }

    LogicValue bit(const Simulator& sim, std::size_t b) const {
        return sim.value(nets.find(spec.names[spec.data[b]])->second);
    }
};

std::filesystem::path TempFile(std::string_view name, std::string_view text) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path) << text;
    return path;
}

}  // namespace

TEST_CASE("rom: catalogue rows and pins", "[parts]") {
    for (const std::string_view name : {"2716", "2513"}) {
        INFO(name);
        const PartInfo* part = FindPart(name);
        REQUIRE(part != nullptr);
        CHECK(part->package == "DIP-24");
        CHECK(part->kind == PartKind::Chip);
        REQUIRE(FindRomSpec(name) != nullptr);
    }
    CHECK(FindRomSpec("74LS00") == nullptr);

    const RomSpec& eprom = RomSpecOf("2716");
    Rom u1("U1", eprom, Pattern(eprom));
    const auto pins = u1.pins();
    REQUIRE(pins.size() == 24);
    CHECK(u1.size() == 2048);
    CHECK(pins[7].name == "A0");
    CHECK(pins[7].kind == PinKind::Input);
    CHECK(pins[8].name == "O0");
    CHECK(pins[8].kind == PinKind::Output);
    CHECK(pins[11].name == "GND");
    CHECK(pins[11].kind == PinKind::Power);
    CHECK(pins[18].name == "A10");
    CHECK(pins[20].name == "VPP");
    CHECK(pins[20].kind == PinKind::Input);
    CHECK(pins[23].name == "VCC");

    const RomSpec& font = RomSpecOf("2513");
    Rom u2("U2", font, Pattern(font));
    const auto fontPins = u2.pins();
    REQUIRE(fontPins.size() == 24);
    CHECK(u2.size() == 512);
    CHECK(fontPins[0].name == "VGG");
    CHECK(fontPins[0].kind == PinKind::NoConnect);
    CHECK(fontPins[1].kind == PinKind::NoConnect);
    CHECK(fontPins[3].name == "O1");
    CHECK(fontPins[3].kind == PinKind::Output);
    CHECK(fontPins[9].name == "GND");
    CHECK(fontPins[9].kind == PinKind::Power);
    CHECK(fontPins[10].name == "CE_N");
    CHECK(fontPins[10].kind == PinKind::Input);
    CHECK(fontPins[11].name == "VDD");
    CHECK(fontPins[11].kind == PinKind::NoConnect);
    CHECK(fontPins[13].name == "A1");
    CHECK(fontPins[21].name == "A9");
}

TEST_CASE("rom: 2716 data follow the address after the access time", "[parts]") {
    // Address 0 → 5 (A0, A2) at 1 µs → 0x405 (A10) at 2 µs.
    Bench b("2716", {{"A0", {Zero, {{1_us, One}}}},
                     {"A2", {Zero, {{1_us, One}}}},
                     {"A10", {Zero, {{2_us, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(900_ns);
    CHECK(b.word(sim) == 0);
    sim.runUntil(1_us + delays::k2716Access - 1);
    CHECK(b.word(sim) == 0);
    sim.runUntil(1_us + delays::k2716Access);
    CHECK(b.word(sim) == 35);
    sim.runUntil(2_us + delays::k2716Access);
    CHECK(b.word(sim) == ((0x405 * 7) & 0xFF));
}

TEST_CASE("rom: 2513 row and character delays", "[parts]") {
    // A1 (row bit) rises at 1 µs; A4 (character bit) at 2 µs; A2 and A5 together at 3 µs.
    Bench b("2513", {{"A1", {Zero, {{1_us, One}}}},
                     {"A4", {Zero, {{2_us, One}}}},
                     {"A2", {Zero, {{3_us, One}}}},
                     {"A5", {Zero, {{3_us, One}}}}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + delays::k2513RowAccess - 1);
    CHECK(b.word(sim) == 0);
    sim.runUntil(1_us + delays::k2513RowAccess);
    CHECK(b.word(sim) == 7);
    sim.runUntil(2_us + delays::k2513CharacterAccess - 1);
    CHECK(b.word(sim) == 7);
    sim.runUntil(2_us + delays::k2513CharacterAccess);
    CHECK(b.word(sim) == ((9 * 7) & 0x1F));
    sim.runUntil(3_us + delays::k2513CharacterAccess - 1);
    CHECK(b.word(sim) == ((9 * 7) & 0x1F));
    sim.runUntil(3_us + delays::k2513CharacterAccess);
    CHECK(b.word(sim) == ((27 * 7) & 0x1F));
}

TEST_CASE("rom: enables float the outputs", "[parts]") {
    SECTION("2716 CE_N and OE_N") {
        Bench b("2716", {{"A0", {One, {}}},
                         {"CE_N", {Zero, {{1_us, One}, {2_us, Zero}}}},
                         {"OE_N", {Zero, {{3_us, One}, {4_us, Zero}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(900_ns);
        CHECK(b.word(sim) == 7);
        sim.runUntil(1_us + delays::k2716Float - 1);
        CHECK(b.word(sim) == 7);
        sim.runUntil(1_us + delays::k2716Float);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(2_us + delays::k2716ChipEnable - 1);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(2_us + delays::k2716ChipEnable);
        CHECK(b.word(sim) == 7);
        sim.runUntil(3_us + delays::k2716Float);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(4_us + delays::k2716OutputEnable - 1);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(4_us + delays::k2716OutputEnable);
        CHECK(b.word(sim) == 7);
    }
    SECTION("2513 CE_N") {
        Bench b("2513", {{"A1", {One, {}}}, {"CE_N", {Zero, {{1_us, One}, {2_us, Zero}}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(900_ns);
        CHECK(b.word(sim) == 7);
        sim.runUntil(1_us + delays::k2513ChipEnable);
        CHECK(sim.isFloating(b.nets.at("O1")));
        sim.runUntil(2_us + delays::k2513ChipEnable - 1);
        CHECK(sim.isFloating(b.nets.at("O1")));
        sim.runUntil(2_us + delays::k2513ChipEnable);
        CHECK(b.word(sim) == 7);
    }
}

TEST_CASE("rom: X inputs", "[parts]") {
    SECTION("an X address bit: data bits where both words agree stay known") {
        // A0 at X: words 0 and 7 (0b00000111) differ in bits 0-2 only.
        Bench b("2716", {{"A0", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        for (std::size_t bit = 0; bit < 3; ++bit) {
            CHECK(b.bit(sim, bit) == X);
        }
        for (std::size_t bit = 3; bit < 8; ++bit) {
            CHECK(b.bit(sim, bit) == Zero);
        }
    }
    SECTION("X on an enable: every data bit X") {
        Bench b("2513", {{"CE_N", {X, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(b.bit(sim, 0) == X);
        CHECK(b.bit(sim, 4) == X);
    }
}

TEST_CASE("rom: the factory reads contents=", "[parts]") {
    const PartInfo* part = FindPart("2513");
    REQUIRE(part != nullptr);
    Circuit circuit;
    const auto none = part->create(circuit, "U1", {});
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error() == "2513 needs contents=<file>");

    const std::vector<PartParam> unknown{{"size", "1"}};
    CHECK(part->create(circuit, "U1", unknown).error() == "2513: unknown parameter 'size'");

    const std::vector<PartParam> missing{{"contents", "/nonexistent/f.rom"}};
    CHECK(part->create(circuit, "U1", missing).error() == "2513: cannot read '/nonexistent/f.rom'");

    const auto wide = TempFile("bw_rom_test_wide.rom", "; too wide\n20\n");
    const std::vector<PartParam> tooWide{{"contents", wide.string()}};
    CHECK(part->create(circuit, "U1", tooWide).error() ==
          "2513: bw_rom_test_wide.rom line 2: word 0x20 is wider than 5 bits");

    const auto good = TempFile("bw_rom_test_good.rom", ".#.#.\n");
    const std::vector<PartParam> contents{{"contents", good.string()}};
    const auto created = part->create(circuit, "U1", contents);
    REQUIRE(created.has_value());
    CHECK(static_cast<Rom*>(*created)->size() == 512);
}
