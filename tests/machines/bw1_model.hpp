#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "parts/mos6502_core.hpp"

namespace bw {

// GETKEY codes of the keys that are not hex digits (KIM listing).
inline constexpr int kKeyAd = 0x10, kKeyDa = 0x11, kKeyPlus = 0x12, kKeyGo = 0x13, kKeyPc = 0x14;

// The KIM-1 as the BW-1's ROMs see it (docs/p5k-bw1-monitor/BW1_MONITOR.md, "The KIM it runs
// on"), around Mos6502Core: one bus cycle a microsecond. Not the netlist (P5l): the 6530s' ports
// and RAM, the 1K RAM, U24, keypad, strap, ST, SST and the six digits, modelled.
class Bw1Model {
public:
    explicit Bw1Model(std::uint64_t seed);  // RAMs random from `seed`; powered on and reset
    void run(std::uint64_t us);
    void reset();  // RS: RES on the 6502 and both 6530s for 8 cycles, then released

    // Keys by GETKEY code (0x00-0x14), one at a time. press(): held `holdUs`, released `gapUs`.
    void hold(int key);
    void release();
    void press(int key, std::uint64_t holdUs = 40'000, std::uint64_t gapUs = 40'000);
    void type(std::string_view keys);          // "AD 0200 DA A9 + 42 GO": key names and hex digits
    void stop(std::uint64_t holdUs = 20'000);  // ST: NMI low while held
    void setSst(bool on) { sst_ = on; }
    void setStrap(bool closed);

    // The six digits over the next `us`: a digit selected under 1/20 of it is ' '; a segment lit
    // for at least half of its digit's time is on. 0-9 A-F, 'P', '-', ' ', else '?' (S and 5 are
    // the same segments: "PASS" reads "PA55").
    std::string display(std::uint64_t us = 20'000);

    std::uint8_t peek(std::uint16_t address) const;  // as the CPU would read it, no side effect
    void poke(std::uint16_t address, std::uint8_t value);       // RAM only
    void stick(std::uint16_t address, unsigned bit, bool one);  // main or RIOT RAM
    void patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes);
    const Mos6502Core& core() const { return core_; }
    bool timerTouched() const { return timerTouched_; }
    std::uint64_t cycles() const { return cycle_; }

private:
    struct Riot {
        std::array<std::uint8_t, 4> reg{};  // port A data, A direction, B data, B direction
    };

    void cycle();
    std::uint8_t read(std::uint16_t address);  // peek(), flagging the timer, charging the bus
    static bool isIo(std::uint16_t address) {  // K5 and A9-A7 = 110: a 6530's ports and timer
        return ((address >> 10) & 7) == 5 && ((address >> 7) & 7) == 6;
    }
    void write(std::uint16_t address, std::uint8_t value);
    std::optional<std::size_t> cell(std::uint16_t address) const;  // RAM byte, if RAM
    std::uint8_t paPins() const;                                   // the 002's
    std::uint8_t pbPins() const;
    int u24() const { return (pbPins() >> 1) & 0xF; }
    void displayChanged();  // the elapsed cycles to the last state's counters, the new state

    Mos6502Core core_;
    std::vector<std::uint8_t> rom002_, rom003_;
    std::array<Riot, 2> riot_{};                                // 0: 002 ($1740), 1: 003 ($1700)
    std::vector<std::uint8_t> cells_, stuckMask_, stuckValue_;  // 1K, then 003 and 002 RAM
    std::uint8_t bus_ = 0;
    std::uint64_t cycle_ = 0;
    std::optional<int> key_;
    bool strap_ = false, sst_ = false, stHeld_ = false, timerTouched_ = false;

    int digit_ = -1;  // lit digit 0-5, or -1
    std::uint8_t segments_ = 0;
    std::uint64_t stateStart_ = 0;
    std::array<std::uint64_t, 6> digitOn_{};
    std::array<std::array<std::uint64_t, 7>, 6> segmentOn_{};
};

}  // namespace bw
