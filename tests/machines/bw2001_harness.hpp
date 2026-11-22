#pragma once

// The BW-2001's firmware under test (docs/p5p-bw2001-firmware/BW2001_FIRMWARE_DESIGN.md §8): keys
// pressed through the PET 2001's matrix, the screen read from the video RAM, and the P5o
// conformance sessions run against either target, the model (bw2001_model) or the netlist.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bw {

// A place in the keyboard matrix: PIA1 PA0-PA3 select `row` 0-9, the key pulls PB `column` low.
struct Bw2001Key {
    int row = 0;
    int column = 0;
    bool operator==(const Bw2001Key&) const = default;
};

inline constexpr Bw2001Key kBw2001Return{6, 5}, kBw2001Stop{9, 4}, kBw2001ShiftLeft{8, 0},
    kBw2001ShiftRight{8, 5}, kBw2001Del{1, 7}, kBw2001CursorDown{1, 6}, kBw2001CursorRight{0, 7},
    kBw2001Home{0, 6}, kBw2001Rvs{9, 0};

// The unshifted key that types `c`: each of the 64 characters $20-$5F has one (the spec's matrix;
// `_` is the left-arrow key, `^` the up-arrow key); nullopt for anything else.
std::optional<Bw2001Key> Bw2001KeyFor(char c);

// The legend the spec gives each matrix place (row 0 PB0 first), "" for an unused place.
std::string_view Bw2001KeyLegend(Bw2001Key key);

// Zero page addresses of the harness signal (machines/130-bw2001/debug-roms/src/bw2001.inc; a test
// checks they agree).
inline constexpr std::uint16_t kBw2001Waiting = 0x40, kBw2001Lines = 0x41;

// What the harness needs of a running BW-2001.
class Bw2001Target {
public:
    virtual ~Bw2001Target() = default;
    virtual void run(std::uint64_t us) = 0;
    virtual void setKey(Bw2001Key key, bool held) = 0;
    virtual std::uint8_t ram(std::uint16_t address) const = 0;  // main RAM
    virtual std::uint8_t screen(std::size_t offset) const = 0;  // video RAM, 0-999
};

// A screen code (bit 7, the reverse video, dropped) as BASIC's character.
char Bw2001ScreenChar(std::uint8_t code);

// A player at the keyboard of a target: keys held 30 ms and released for 30 ms.
class Bw2001Typist {
public:
    explicit Bw2001Typist(Bw2001Target& target) : target_(target) {}

    void press(Bw2001Key key, bool shift = false);
    void type(std::string_view text);   // each character's unshifted key
    void enter(std::string_view text);  // type, then RETURN
    // Runs until the editor waits for a line (after an enter, one it has not yet counted); false
    // after `limitUs` without.
    bool waitForLine(std::uint64_t limitUs = 5'000'000);
    // The 25 rows, trailing spaces dropped; blank rows at the end dropped.
    std::vector<std::string> rows() const;
    Bw2001Target& target() { return target_; }

private:
    Bw2001Target& target_;
    std::optional<std::uint8_t> last_;  // LINES read before the last enter's RETURN
};

// The address `{RAM}` stands for in the sessions (USRRAM,
// machines/130-bw2001/debug-roms/src/bw2001.inc).
inline constexpr int kBw2001SessionRam = 1008;

// Runs a P5o conformance session (tests/data/basic/) from power-on, compares the full view and
// returns the first difference, or nullopt when the screen matches.
std::optional<std::string> RunBw2001Session(Bw2001Target& target,
                                            const std::filesystem::path& path);

// The conformance sessions, sorted.
std::vector<std::filesystem::path> Bw2001Sessions();

}  // namespace bw
