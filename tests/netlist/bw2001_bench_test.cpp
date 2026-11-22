// The P5n bench (tests/data/bw2001_bench.bwn): the PET 2001's bus, running
// machines/130-bw2001/debug-roms/bw2001_bench.asm, and the real BASIC 1 when the user has placed it
// (external/roms/pet, tests/data/pet_roms.md).

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "external_rom.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/ram2114.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kDone = 400_ms;          // the RAM tests pass at about 350 ms
constexpr Time kHalfPeriod = 1'000_us;  // T1: N + 2 = 1000 cycles at 1 MHz

std::filesystem::path BenchPath() {
    return std::filesystem::path(BW_ROOT_DIR) / "tests/data/bw2001_bench.bwn";
}

struct Bench {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;

    // `basic1`: the seven sockets hold the real ROMs (the caller checked they exist).
    explicit Bench(std::uint64_t seed, std::span<const StuckCell> cells = {}, bool basic1 = false) {
        std::ifstream in(BenchPath());
        REQUIRE(in);
        std::stringstream text;
        text << in.rdbuf();
        auto netlist = ParseNetlist(text.str(), "bw2001_bench.bwn");
        REQUIRE(netlist.has_value());
        if (basic1) {
            for (PartDecl& part : netlist->parts) {
                for (const PetRom& rom : kPetBasic1) {
                    if (part.reference != rom.socket) {
                        continue;
                    }
                    for (PartParam& param : part.params) {
                        if (param.name == "contents") {
                            param.value = ExternalRom(rom.file)->string();
                        }
                    }
                }
            }
        }
        ResolveContentsPaths(*netlist, BenchPath().parent_path());
        auto result = BuildCircuit(*netlist, {}, {}, cells);
        REQUIRE(result.has_value());
        loaded = std::move(*result);
        CHECK(loaded->warnings.empty());
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        for (const char* net : {"IRQ_N", "VPB7", "VCB1", "VCB2", "RETRACE"}) {
            sim->record(loaded->nets.at(net));  // after powerOn, which clears the recorder
        }
    }

    NetId net(const std::string& name) const { return loaded->nets.at(name); }
    LogicValue value(const std::string& name) const { return sim->value(net(name)); }

    // Nets `stem`0..`stem`7 as a byte; -1 if one is not 0 or 1.
    int byte(const std::string& stem) const {
        int result = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const LogicValue v = value(stem + std::to_string(bit));
            if (v != Zero && v != One) {
                return -1;
            }
            result |= (v == One ? 1 : 0) << bit;
        }
        return result;
    }

    // The times `name` changed to `to` from `from` on.
    std::vector<Time> edges(const std::string& name, LogicValue to, Time from) const {
        std::vector<Time> times;
        LogicValue last = X;
        for (const Sample& s : sim->recorder().samples(net(name))) {
            if (s.time >= from && s.value == to && last != to && last != X) {
                times.push_back(s.time);
            }
            last = s.value;
        }
        return times;
    }

    void press(const std::string& reference, bool down) {
        Switch* key = SwitchOf(*loaded, reference);
        REQUIRE(key != nullptr);
        key->setPressed(down);
        sim->wake(key->id());
    }

    // The video RAM's byte at $8000 + `offset`: UC3 the low nibble, UC4 the high.
    unsigned screen(std::size_t offset) const {
        const auto* low = dynamic_cast<const Ram2114*>(loaded->components.at("UC3"));
        const auto* high = dynamic_cast<const Ram2114*>(loaded->components.at("UC4"));
        return low->word(offset) | high->word(offset) << 4;
    }
};

PinIndex PinOf(const Bench& bench, const std::string& reference, std::string_view pin) {
    return bench.loaded->components.at(reference)->pinIndex(pin);
}

// A PET screen code as ASCII: $01-$1A letters, $20-$3F as ASCII, $00 '@'; others '?'.
char Ascii(unsigned code) {
    code &= 0x7F;
    if (code == 0) {
        return '@';
    }
    if (code <= 0x1A) {
        return static_cast<char>('A' + code - 1);
    }
    if (code >= 0x20 && code <= 0x3F) {
        return static_cast<char>(code);
    }
    return '?';
}

}  // namespace

TEST_CASE("bw2001 bench: RAM passes, T1, the shift register, the retrace and the keys",
          "[netlist][slow]") {
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO("seed " << seed);
        Bench bench(seed);
        bench.sim->runUntil(kDone);
        CHECK(bench.byte("P2PB") == 0x55);

        // T1: PB7 a square wave of 1000 us halves; each turn is T1's interrupt, which the handler
        // clears at once (skipping those that land while the retrace's handler runs).
        bench.sim->runUntil(kDone + 60_ms);
        const auto pb7 = bench.edges("VPB7", One, kDone);
        const auto pb7Low = bench.edges("VPB7", Zero, kDone);
        REQUIRE(pb7.size() >= 20);
        REQUIRE(pb7Low.size() >= 20);
        for (std::size_t i = 1; i < pb7.size(); ++i) {
            CHECK(pb7[i] - pb7[i - 1] == 2 * kHalfPeriod);
        }
        CHECK((pb7Low[0] > pb7[0] ? pb7Low[0] - pb7[0] : pb7[0] - pb7Low[0]) == kHalfPeriod);
        const auto retraces = bench.edges("RETRACE", Zero, kDone);
        const auto irqLow = bench.edges("IRQ_N", Zero, kDone);
        const auto irqHigh = bench.edges("IRQ_N", One, kDone);
        int checked = 0;
        for (const Time turn : pb7) {
            bool nearRetrace = false;
            for (const Time r : retraces) {
                nearRetrace |= turn >= r && turn < r + 300_us;
            }
            if (nearRetrace) {
                continue;
            }
            bool low = false, released = false;
            for (const Time t : irqLow) {
                low |= t <= turn && turn - t < 2_us;
            }
            for (const Time t : irqHigh) {
                released |= t > turn && t - turn < 30_us;
            }
            CHECK(low);
            CHECK(released);
            ++checked;
        }
        CHECK(checked >= 20);

        // The shift register, mode 4 at T2 latch 8: CB1 every 10 us, CB2 4 bits low, 4 high.
        const auto cb1 = bench.edges("VCB1", Zero, kDone);
        REQUIRE(cb1.size() >= 100);
        for (std::size_t i = 1; i < 100; ++i) {
            CHECK(cb1[i] - cb1[i - 1] == 20_us);
        }
        const auto cb2 = bench.edges("VCB2", One, kDone);
        REQUIRE(cb2.size() >= 10);
        for (std::size_t i = 1; i < 10; ++i) {
            CHECK(cb2[i] - cb2[i - 1] == 160_us);
        }

        // The retrace: the count on the VIA's port A steps once a fall.
        const int count = bench.byte("VPA");
        const Time start = bench.sim->now();
        bench.sim->runUntil(start + 55_ms);
        const auto falls = bench.edges("RETRACE", Zero, start);
        CHECK(falls.size() == 3);
        CHECK(bench.byte("VPA") == ((count + static_cast<int>(falls.size())) & 0xFF));

        // The keys, read at each retrace: SW1 (row 3, column 5) on PIA2 PA5, SW2 (row 8, column 0)
        // on PIA2 CA2.
        CHECK(bench.value("P2PA5") == One);
        CHECK(bench.value("P2CA2") == One);
        bench.press("SW1", true);
        bench.press("SW2", true);
        bench.sim->runUntil(bench.sim->now() + 20_ms);
        CHECK(bench.value("P2PA5") == Zero);
        CHECK(bench.value("P2PA0") == One);
        CHECK(bench.value("P2CA2") == Zero);
        bench.press("SW1", false);
        bench.press("SW2", false);
        bench.sim->runUntil(bench.sim->now() + 20_ms);
        CHECK(bench.value("P2PA5") == One);
        CHECK(bench.value("P2CA2") == One);
    }
}

TEST_CASE("bw2001 bench: stuck RAM and video RAM cells fail their tests", "[netlist][slow]") {
    // $1123 (UI5 word $123) holds $23 ^ $11 ^ $A5 = $97: bit 0 is 1, stuck at 0.
    // $8010 (UC4 word $010) holds $10 ^ $80 ^ $A5 = $35: bit 7 is 0, stuck at 1.
    Bench probe(1);
    const StuckCell ram[] = {{"UI5", PinOf(probe, "UI5", "IO1"), std::size_t{0x123}, Zero}};
    Bench bad(1, ram);
    bad.sim->runUntil(kDone);
    CHECK(bad.byte("P2PB") == 0x01);
    const StuckCell video[] = {{"UC4", PinOf(probe, "UC4", "IO4"), std::size_t{0x010}, One}};
    Bench badVideo(1, video);
    badVideo.sim->runUntil(kDone);
    CHECK(badVideo.byte("P2PB") == 0x02);
}

TEST_CASE("bw2001 bench: the real BASIC 1 boots to READY.", "[netlist][slow]") {
    for (const PetRom& rom : kPetBasic1) {
        if (!ExternalRom(rom.file)) {
            SKIP(kNoPetRoms);
        }
    }
    Bench bench(1, {}, true);
    bench.sim->runUntil(600_ms);
    std::vector<std::string> rows;
    for (std::size_t row = 0; row < 25; ++row) {
        std::string text;
        for (std::size_t column = 0; column < 40; ++column) {
            text += Ascii(bench.screen(row * 40 + column));
        }
        rows.push_back(text);
    }
    std::string screen;
    for (const std::string& row : rows) {
        screen += row + "\n";
    }
    INFO(screen);
    CHECK(rows[0].starts_with("*** COMMODORE BASIC ***"));
    CHECK(rows[2].starts_with(" 7167 BYTES FREE"));
    CHECK(rows[4].starts_with("READY."));
}
