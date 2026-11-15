#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

#include "bw2001_harness.hpp"
#include "parts/mos6502_core.hpp"

namespace bw {

// The PET 2001 as the BW-2001's firmware sees it (docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md,
// "The PET it runs on"), around Mos6502Core: one bus cycle a microsecond. Not the netlist: the
// RAM, the video RAM, the ROM windows, PIA1 (keyboard rows, DIAG, the columns, the retrace on CB1
// at the start of each 16 640-cycle frame), the VIA's PB5 (low during the frame's first 3 840
// cycles, the 60 blank lines: P5q) and PCR, and the key matrix, modelled. PIA2, the VIA's other
// registers and empty sockets are flagged.
class Bw2001Model final : public Bw2001Target {
public:
    struct Options {
        std::uint64_t seed = 1;
        std::uint32_t ramTop = 0x2000;  // RAM is $0000 to ramTop - 1, at most 8 KB
        bool diag = false;              // true: DIAG (PA7) held low from power-on
        // Each ROM window by its base ($C000, $E000, $F000); empty: the three committed
        // roms/bw2001_*.rom. A window not given is empty sockets.
        std::map<std::uint16_t, std::vector<std::uint8_t>> roms;
    };

    explicit Bw2001Model(Options options);  // RAM random from the seed; powered on and reset
    void run(std::uint64_t us) override;
    void reset();  // RES low for 8 cycles, then released
    void setKey(Bw2001Key key, bool held) override;
    void setDiag(bool low) { diag_ = low; }

    std::uint8_t ram(std::uint16_t address) const override { return peek(address); }
    std::uint8_t screen(std::size_t offset) const override { return video_[offset & 0x3FF]; }
    std::uint8_t peek(std::uint16_t address) const;  // as the CPU would read it, no side effect
    void poke(std::uint16_t address, std::uint8_t value);       // RAM and video RAM
    void stick(std::uint16_t address, unsigned bit, bool one);  // RAM and video RAM
    void deadRam() { dead_ = true; }                            // every RAM read $FF
    void patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes);

    bool touchedUnmodelled() const { return touched_; }  // PIA2, the VIA, an empty socket
    // Reads and writes of the screen ($8000-$8FFF) while the beam draws the text: the PET's snow.
    std::uint64_t snowAccesses() const { return snow_; }
    std::uint8_t viaPcr() const { return pcr_; }
    const Mos6502Core& core() const { return core_; }
    std::uint64_t cycles() const { return cycle_; }
    // The lowest stack pointer since power-on, or since the last resetStackMark().
    std::uint8_t stackMark() const { return stackMark_; }
    void resetStackMark() { stackMark_ = 0xFF; }

private:
    struct Window {
        std::uint16_t base = 0;
        std::vector<std::uint8_t> words;
        std::vector<bool> filled;  // per 2 KB chip: holds anything but $FF
    };

    void cycle();
    std::uint8_t read(std::uint16_t address);  // with the PIA's side effects
    void write(std::uint16_t address, std::uint8_t value);
    std::uint8_t ramRead(std::uint16_t address) const;
    std::uint8_t videoRead(std::uint16_t address) const;
    const Window* windowOf(std::uint16_t address) const;
    std::uint8_t paPins() const;
    std::uint8_t pbPins() const;

    Mos6502Core core_;
    std::uint32_t ramTop_;
    std::vector<Window> roms_;
    std::vector<std::uint8_t> ram_, ramMask_, ramValue_;  // stuck bits: mask, value
    std::array<std::uint8_t, 1024> video_{}, videoMask_{}, videoValue_{};
    std::array<std::uint8_t, 10> held_{};  // per row, the columns held
    bool diag_ = false, dead_ = false, touched_ = false;

    // PIA1 (R6520, P5n §5.2), as far as the firmware uses it.
    std::uint8_t ora_ = 0, ddra_ = 0, cra_ = 0, orb_ = 0, ddrb_ = 0, crb_ = 0;
    std::uint8_t pcr_ = 0;    // the VIA's PCR: CA2 (GRAPHIC) by the firmware
    std::uint64_t snow_ = 0;  // snowAccesses()
    bool inText() const;      // the beam inside the 40 x 25 text now
    bool blank() const;       // PB5 low: the 60 lines outside the text

    std::uint8_t bus_ = 0;
    std::uint64_t cycle_ = 0;
    std::uint8_t stackMark_ = 0xFF;
};

}  // namespace bw
