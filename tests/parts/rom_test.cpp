#include "parts/rom.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

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
    Rom* rom = nullptr;

    Bench(std::string_view part, std::map<std::string, Input> inputs, unsigned activeHigh = 0)
        : spec(RomSpecOf(part)) {
        auto& chip = circuit.add<Rom>("U1", spec, Pattern(spec), activeHigh);
        rom = &chip;
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

TEST_CASE("rom: 2316B selects at the levels select= sets", "[parts]") {
    // Every select= mask against every level of CS1-CS3: on only where each select is active.
    const unsigned mask = GENERATE(range(0u, 8u));
    INFO("activeHigh " << mask);
    for (unsigned levels = 0; levels < 8; ++levels) {
        INFO("levels " << levels);
        const auto level = [&](unsigned i) { return (levels >> i) & 1u ? One : Zero; };
        Bench b("2316B",
                {{"A0", {One, {}}},
                 {"CS1", {level(0), {}}},
                 {"CS2", {level(1), {}}},
                 {"CS3", {level(2), {}}}},
                mask);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        if (levels == mask) {
            CHECK(b.word(sim) == 7);
        } else {
            CHECK(sim.isFloating(b.nets.at("O0")));
        }
    }
}

TEST_CASE("rom: 2316B delays and X select", "[parts]") {
    SECTION("access, select and float") {
        // select=LHH: CS1 low, CS2 and CS3 high select it.
        Bench b("2316B",
                {{"A0", {Zero, {{3_us, One}}}},
                 {"CS1", {Zero, {{1_us, One}, {2_us, Zero}}}},
                 {"CS2", {One, {}}},
                 {"CS3", {One, {}}}},
                0b110);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(900_ns);
        CHECK(b.word(sim) == 0);
        sim.runUntil(1_us + delays::k2316Float - 1);
        CHECK(b.word(sim) == 0);
        sim.runUntil(1_us + delays::k2316Float);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(2_us + delays::k2316Select - 1);
        CHECK(sim.isFloating(b.nets.at("O0")));
        sim.runUntil(2_us + delays::k2316Select);
        CHECK(b.word(sim) == 0);
        sim.runUntil(3_us + delays::k2316Access - 1);
        CHECK(b.word(sim) == 0);
        sim.runUntil(3_us + delays::k2316Access);
        CHECK(b.word(sim) == 7);
    }
    SECTION("an X select: every data bit X") {
        Bench b("2316B", {{"CS2", {X, {}}}}, 0);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(b.bit(sim, 0) == X);
        CHECK(b.bit(sim, 7) == X);
    }
}

TEST_CASE("rom: 2316B select= parameter", "[parts]") {
    const PartInfo* part = FindPart("2316B");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-24");
    const std::string empty = std::string(BW_ROOT_DIR) + "/tests/data/empty.rom";
    Circuit circuit;
    const auto make = [&](std::vector<PartParam> params) {
        return part->create(circuit, "U1", params);
    };
    CHECK(make({{"contents", empty}}).error() ==
          "2316B needs select=<CS1 CS2 CS3: L or H each>, e.g. select=LLH");
    CHECK(make({{"select", "LX"}, {"contents", empty}}).error() ==
          "2316B: bad select 'LX' (three of L or H)");
    CHECK(make({{"select", "LLHH"}, {"contents", empty}}).error() ==
          "2316B: bad select 'LLHH' (three of L or H)");
    CHECK(make({{"select", "LLH"}}).error() == "2316B needs contents=<file>");
    REQUIRE(make({{"select", "HLH"}, {"contents", empty}}).has_value());
    const std::vector<PartParam> onEprom{{"select", "LLH"}, {"contents", empty}};
    CHECK(FindPart("2716")->create(circuit, "U2", onEprom).error() ==
          "2716: unknown parameter 'select'");

    const RomSpec& spec = RomSpecOf("2316B");
    Rom rom("U3", spec, Pattern(spec));
    const auto pins = rom.pins();
    REQUIRE(pins.size() == 24);
    CHECK(pins[17].name == "CS2");
    CHECK(pins[19].name == "CS1");
    CHECK(pins[20].name == "CS3");
    CHECK(pins[20].kind == PinKind::Input);
    CHECK(pins[18].name == "A10");
}

TEST_CASE("rom: 2332 selects at the levels select= sets", "[parts]") {
    const unsigned mask = GENERATE(range(0u, 4u));
    INFO("activeHigh " << mask);
    for (unsigned levels = 0; levels < 4; ++levels) {
        INFO("levels " << levels);
        const auto level = [&](unsigned i) { return (levels >> i) & 1u ? One : Zero; };
        Bench b("2332", {{"A0", {One, {}}}, {"CS1", {level(0), {}}}, {"CS2", {level(1), {}}}},
                mask);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        if (levels == mask) {
            CHECK(b.word(sim) == 7);
        } else {
            CHECK(sim.isFloating(b.nets.at("O0")));
        }
    }
}

TEST_CASE("rom: 2332 delays and its twelve address bits", "[parts]") {
    // select=LL; A11 high at 3 us: word $800, (0x800 * 7) & 0xFF = 0.
    Bench b("2332",
            {{"A0", {One, {}}},
             {"A11", {Zero, {{3_us, One}}}},
             {"CS1", {Zero, {{1_us, One}, {2_us, Zero}}}},
             {"CS2", {Zero, {}}}},
            0);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(900_ns);
    CHECK(b.word(sim) == 7);
    sim.runUntil(1_us + delays::k2332Float - 1);
    CHECK(b.word(sim) == 7);
    sim.runUntil(1_us + delays::k2332Float);
    CHECK(sim.isFloating(b.nets.at("O0")));
    sim.runUntil(2_us + delays::k2332Select - 1);
    CHECK(sim.isFloating(b.nets.at("O0")));
    sim.runUntil(2_us + delays::k2332Select);
    CHECK(b.word(sim) == 7);
    sim.runUntil(3_us + delays::k2332Access - 1);
    CHECK(b.word(sim) == 7);
    sim.runUntil(3_us + delays::k2332Access);
    CHECK(b.word(sim) == ((0x801 * 7) & 0xFF));
}

TEST_CASE("rom: 2332 pins and select= parameter", "[parts]") {
    const PartInfo* part = FindPart("2332");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-24");
    const std::string empty = std::string(BW_ROOT_DIR) + "/tests/data/empty.rom";
    Circuit circuit;
    const auto make = [&](std::vector<PartParam> params) {
        return part->create(circuit, "U1", params);
    };
    CHECK(make({{"contents", empty}}).error() ==
          "2332 needs select=<CS1 CS2: L or H each>, e.g. select=LH");
    CHECK(make({{"select", "LLH"}, {"contents", empty}}).error() ==
          "2332: bad select 'LLH' (two of L or H)");
    CHECK(make({{"select", "LX"}, {"contents", empty}}).error() ==
          "2332: bad select 'LX' (two of L or H)");
    REQUIRE(make({{"select", "LL"}, {"contents", empty}}).has_value());

    const RomSpec& spec = RomSpecOf("2332");
    CHECK(spec.address.size() == 12);
    Rom rom("U3", spec, Pattern(spec));
    const auto pins = rom.pins();
    REQUIRE(pins.size() == 24);
    CHECK(pins[17].name == "A11");
    CHECK(pins[18].name == "A10");
    CHECK(pins[19].name == "CS1");
    CHECK(pins[20].name == "CS2");
    CHECK(pins[21].name == "A9");
    CHECK(pins[22].name == "A8");
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

TEST_CASE("rom: offset= takes one chip's slice of a larger .rom", "[parts]") {
    const PartInfo* part = FindPart("2316B");
    REQUIRE(part != nullptr);
    // 4 KB: $11 at word 0, $22 at $7FF, $33 at $800, $44 at $FFF.
    const auto big = TempFile("bw_rom_test_big.rom", "@000 11\n@7FF 22\n@800 33\n@FFF 44\n");
    Circuit circuit;
    const auto chip = [&](std::string offset) {
        std::vector<PartParam> params{{"select", "LLH"}, {"contents", big.string()}};
        if (!offset.empty()) {
            params.push_back({"offset", std::move(offset)});
        }
        return part->create(circuit, "U1", params);
    };
    const auto low = chip("0");
    REQUIRE(low.has_value());
    const auto* lowRom = static_cast<Rom*>(*low);
    CHECK(lowRom->size() == 2048);
    CHECK(lowRom->word(0) == 0x11);
    CHECK(lowRom->word(0x7FF) == 0x22);
    const auto high = chip("$800");
    REQUIRE(high.has_value());
    const auto* highRom = static_cast<Rom*>(*high);
    CHECK(highRom->word(0) == 0x33);
    CHECK(highRom->word(0x7FF) == 0x44);
    CHECK(static_cast<Rom*>(*chip("2048"))->word(0) == 0x33);
    // Past the file's last word: an erased chip.
    CHECK(static_cast<Rom*>(*chip("$1000"))->word(0) == 0xFF);
    // Without offset= the file must fit the chip, as before.
    CHECK(chip("").error().find("past the end") != std::string::npos);

    CHECK(chip("$80G").error() == "2316B: bad offset '$80G'");
    CHECK(chip("100").error() == "2316B: offset must be a multiple of 2048 below 65536");
    CHECK(chip("$10000").error() == "2316B: offset must be a multiple of 2048 below 65536");
    const auto dump = TempFile("bw_rom_test_dump.bin", std::string(2048, '\0'));
    const std::vector<PartParam> raw{
        {"select", "LLH"}, {"contents", dump.string()}, {"offset", "0"}};
    CHECK(part->create(circuit, "U2", raw).error() == "2316B: offset= is for .rom text files");
}

TEST_CASE("rom: stuck cell and column", "[parts]") {
    {
        // Address 5 holds 35 = 0010 0011; O1 stuck at 0 there gives 33.
        Bench bench("2716", {{"A0", {One, {}}}, {"A2", {One, {}}}});
        CHECK(bench.rom->wordCount(0) == 2048);
        const Component& chip = *bench.rom;
        CHECK(bench.rom->dataBit(chip.pinIndex("O3")) == 3u);
        CHECK_FALSE(bench.rom->dataBit(chip.pinIndex("A0")).has_value());
        CHECK(bench.rom->wordBits(0) == 8);
        CHECK(bench.rom->read(0, 5) == 35);
        bench.rom->stick(0, 5, 1, false);
        CHECK(bench.rom->read(0, 5) == 33);  // P6p: read through the stuck cell
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        sim.runUntil(2_us);
        CHECK(bench.word(sim) == 33);
    }
    for (const bool high : {false, true}) {
        // 2513, O1 stuck at 0 in every word: addresses 1 (7) and 3 (21) read 6 and 20.
        Bench bench("2513", {{"A1", {One, {}}}, {"A2", {Zero, {{3_us, One}}}}});
        const Component& chip = *bench.rom;
        CHECK(bench.rom->dataBit(chip.pinIndex("O1")) == 0u);
        CHECK(bench.rom->wordBits(0) == 5);
        for (std::size_t word = 0; word < bench.rom->wordCount(0); ++word) {
            bench.rom->stick(0, word, 0, false);
        }
        CHECK(bench.rom->read(0, 1) == 6);
        Simulator sim(bench.circuit);
        sim.powerOn(high ? 2 : 1);
        sim.runUntil(2_us);
        CHECK(bench.word(sim) == 6);
        sim.runUntil(5_us);
        CHECK(bench.word(sim) == 20);
    }
}

TEST_CASE("rom: a select after the address waits for the address's access time", "[parts]") {
    // 2332 select=LL. A11 high at 1 us while deselected (CS1 high); CS1 low at 1.1 us: the data
    // at 1 us + access (the later), not 1.1 us + select. CS1 high at 2 us, A11 low at 3 us, CS1
    // low at 3.4 us: the address is ready by then, the data at 3.4 us + select.
    Bench b("2332",
            {{"A0", {One, {}}},
             {"A11", {Zero, {{1_us, One}, {3_us, Zero}}}},
             {"CS1", {One, {{1100_ns, Zero}, {2_us, One}, {3400_ns, Zero}}}},
             {"CS2", {Zero, {}}}},
            0);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + delays::k2332Access - 1);
    CHECK(sim.isFloating(b.nets.at("O0")));
    sim.runUntil(1_us + delays::k2332Access);
    CHECK(b.word(sim) == ((0x801 * 7) & 0xFF));
    sim.runUntil(3400_ns + delays::k2332Select - 1);
    CHECK(sim.isFloating(b.nets.at("O0")));
    sim.runUntil(3400_ns + delays::k2332Select);
    CHECK(b.word(sim) == 7);
}

TEST_CASE("rom: a select after a quiet spell times the data from the last address change",
          "[parts]") {
    // 2513, off until 2.01 us. A4 (a character bit, 500 ns) rises at 1 us, A1 (a row bit,
    // 450 ns) at 2 us: the word is ready at 2 us + the row access, the delay of the last change
    // alone, not 2 us + the character access.
    Bench b("2513", {{"A1", {Zero, {{2_us, One}}}},
                     {"A4", {Zero, {{1_us, One}}}},
                     {"CE_N", {One, {{2010_ns, Zero}}}}});
    const NetId o0 = b.nets.at(std::string(b.spec.names[b.spec.data[0]]));
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(2_us + delays::k2513RowAccess - 1);
    CHECK(sim.isFloating(o0));
    sim.runUntil(2_us + delays::k2513RowAccess);
    CHECK(b.word(sim) == ((9 * 7) & 0x1F));
}

TEST_CASE("rom: off, address changes do not evaluate it; with an X enable they do", "[parts]") {
    std::vector<Stimulus::Step> toggles;
    for (int i = 1; i <= 10; ++i) {
        toggles.push_back({i * 1_us, i % 2 ? One : Zero});
    }
    std::uint64_t evaluations[2] = {};
    for (const int x : {0, 1}) {
        Bench b("2716", {{"A3", {Zero, toggles}}, {"CE_N", {x ? LogicValue::X : One, {}}}});
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(20_us);
        evaluations[x] = sim.stats().evaluations;
    }
    CHECK(evaluations[1] - evaluations[0] == 10);
}
