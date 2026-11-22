#pragma once

// Machine 130 BW-2001 under test (machines/130-bw2001/docs/BW2001_DESIGN.md): the Machine
// running with its TV on the MONITOR connector, typed on through J5, its screen read from the
// video RAM. Shared by the Machine's fault-free cases and its Puzzles.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "bw2001_harness.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/ram2114.hpp"
#include "sim/simulator.hpp"

namespace bw {

std::filesystem::path Bw2001NetlistPath();

// The Circuit built from the Board's Copper, with no Faults: the Machine as it ships, on
// Commodore's BASIC 1 (roms/).
std::unique_ptr<LoadedCircuit> Bw2001BoardCircuit();

// The same on our own firmware (debug-roms/, --roms): its banner, its self-test, and the
// harness signal the typist waits on.
std::unique_ptr<LoadedCircuit> Bw2001DebugCircuit();

// The Puzzle's or repaired Machine's Circuit (REQUIREd).
std::unique_ptr<LoadedCircuit> Bw2001Built(const Machine& machine);

// Screen accesses while the beam draws the text (SEL8, VIDEO_ON, DIS_ON and BLANK_TV all high):
// the PET's snow. BLANK_TV low hides it: BASIC 1 blanks the picture while it scrolls. Fed each
// slice's samples before they are trimmed. The Nets are found by their drivers' pins: on a damaged
// Board Nets are named from the Copper.
struct SnowWatch {
    NetId sel8, videoOn, disOn, blankTv;
    LogicValue s = LogicValue::Zero, v = LogicValue::Zero, d = LogicValue::Zero,
               b = LogicValue::Zero;
    Time fedUntil = -1;
    int accesses = 0;  // rises into all three high
    int all = 0;       // SEL8's rises: every screen access

    explicit SnowWatch(const LoadedCircuit& loaded);
    void feed(const Simulator& sim);
};

// The Machine running, its TV on the MONITOR connector; a harness target (P5p's typist and
// sessions run on it as on the model).
struct Running final : Bw2001Target {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;
    std::unique_ptr<TvInput> tv;
    std::optional<SnowWatch> snow;
    const Ram2114* videoLow = nullptr;
    const Ram2114* videoHigh = nullptr;
    std::vector<const Ram2114*> low, high;

    Running(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit, bool watchSnow = false);
    void runUntil(Time t);
    void run(std::uint64_t us) override;
    void setKey(Bw2001Key key, bool held) override;
    std::uint8_t ram(std::uint16_t address) const override;
    std::uint8_t screen(std::size_t offset) const override;
};

// Our firmware's banner (Bw2001DebugCircuit) and Commodore's (BASIC 1, the Machine's own ROMs).
inline const std::vector<std::string> kBw2001Banner{"*** BW-2001 TINY BASIC ***", "",
                                                    "7166 BYTES FREE", "", "READY"};
inline const std::vector<std::string> kPetBanner{"*** COMMODORE BASIC ***", "", " 7167 BYTES FREE",
                                                 "", "READY."};

// The video RAM as text rows, as typist.rows() (P5p's screen codes; BASIC 1 uses the same).
std::vector<std::string> Bw2001ScreenRows(const Running& m);

// Types `text` on J5 (40 ms down, 40 ms up a character) and runs until it is all typed.
void Bw2001Type(Running& m, std::string_view text);

// Runs, a millisecond at a time, until the video RAM's rows are `rows`; false after `limit`.
// Commodore's BASIC 1 has no harness signal: the screen is all a test can wait on.
bool Bw2001WaitForRows(Running& m, const std::vector<std::string>& rows, Time limit);

}  // namespace bw
