#pragma once

// Machine 120 BW-1 under test (docs/machines/120-bw1/BW1_DESIGN.md): a powered BW-1, its keypad
// pressed as a player would, its digits read as the player reads them. Shared by the Machine's
// fault-free cases and its Puzzles.

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

namespace bw {

std::filesystem::path Bw1NetlistPath();

// The switch reference of a key name: a hex digit, AD, DA, +, GO, PC, ST or RS.
std::string Bw1KeyReference(std::string_view name);

// The digits are read over 20 ms: about six scans.
inline constexpr Time kBw1Window = [] {
    using namespace literals;
    return 20_ms;
}();

// A powered BW-1 and its keypad. Digits and segments are found by their pins (DS1-DS6), not by
// Net name: on a damaged Board Nets are named from the Copper.
class Bw1 {
public:
    Bw1(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed);

    Time now() const { return sim_.now(); }
    void run(Time time) { sim_.runUntil(sim_.now() + time); }
    void runUntil(Time time) { sim_.runUntil(time); }
    const LoadedCircuit& loaded() const { return *loaded_; }
    Simulator& sim() { return sim_; }

    void set(std::string_view reference, bool pressed);
    void hold(std::string_view key) { set(Bw1KeyReference(key), true); }
    void release(std::string_view key) { set(Bw1KeyReference(key), false); }
    void press(std::string_view key);
    // "AD 0300 DA A9 +": key names, and hex digits one a key; blanks separate.
    void type(std::string_view keys);
    void setSst(bool on) { set("SW24", on); }

    // The six digits over the next `window`: a digit is read while it alone is selected; a blank
    // for a digit selected less than 1/20 of the time, '?' for segments of no known character.
    std::string display(Time window = kBw1Window);
    // Each digit's lit segments over the next `window` (bit s: segment 'A' + s), read while that
    // digit is selected, whatever other digits are: a digit selected with others shows theirs too.
    std::array<std::uint8_t, 6> segments(Time window = kBw1Window);

private:
    std::unique_ptr<LoadedCircuit> loaded_;
    Simulator sim_;
    std::array<NetId, 6> digits_{};
    std::array<std::array<NetId, 7>, 6> segments_{};
};

bool IsHex(std::string_view text);

// "AAAA" and two hex digits: the address shown with the byte there.
void CheckShows(const std::string& shown, std::string_view address);

// After power-on: the reset is over at 20 ms, the monitor shows 0200 by about 25 ms.
Bw1 Started(std::unique_ptr<LoadedCircuit> loaded, std::uint64_t seed);

// A Machine's Circuit for one power-on; with `strap`, a Bodge Wire from U24 pad 4 (STRAP) to U2
// pad 2 (PA0) while it is built (the Machine is left as it was).
std::unique_ptr<LoadedCircuit> Bw1Circuit(Machine& machine, bool strap = false);

}  // namespace bw
