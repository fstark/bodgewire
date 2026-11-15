#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "parts/switch.hpp"
#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// One key of the BW-2001 keyboard: where it is in the matrix and what its cap says.
struct MatrixKey {
    std::uint8_t row;          // 0-9: the 74LS145 output that scans it
    std::uint8_t column;       // 0-7: PIA1 PB0-PB7
    std::string_view legend;   // "A", "RETURN", "CRSR RT"
    std::string_view shifted;  // "" or what SHIFT makes of it ("CLR", "INST", "CRSR LF", "CRSR UP")
};

// The PET 2001's 73 keys (P5p's matrix, BW2001_FIRMWARE.md), row 0 column 0 first; the index of
// a key in this span is its index everywhere (setHeld, MatrixPress).
std::span<const MatrixKey> Bw2001Keys();
std::optional<std::size_t> Bw2001KeyAt(int row, int column);
inline constexpr std::size_t kBw2001KeyCount = 73;

// Characters for the PET's named keys (Unicode private use), for MatrixKeyFor and the PC keys.
inline constexpr char32_t kKeyHome = 0xE000, kKeyClr = 0xE001, kKeyDown = 0xE002, kKeyUp = 0xE003,
                          kKeyRight = 0xE004, kKeyLeft = 0xE005, kKeyDel = 0xE006,
                          kKeyInst = 0xE007, kKeyStop = 0xE008, kKeyRvs = 0xE009;

// A key and whether SHIFT (left) is held with it.
struct MatrixPress {
    std::size_t key;
    bool shift = false;
};

// The press that types `c` on the BW-2001 keyboard: 0x20-0x5F by the key whose legend it is ('_'
// the left-arrow key, '^' the up-arrow key), a-z as A-Z, '\n' or '\r' RETURN, the kKey… named keys
// (CLR, INST, CRSR LF, CRSR UP with SHIFT); nullopt for anything else.
std::optional<MatrixPress> MatrixKeyFor(char32_t c);

inline constexpr Time kMatrixHold = 40'000'000'000;  // ps: a typed key held 40 ms
inline constexpr Time kMatrixGap = 40'000'000'000;   // ps: then released 40 ms
inline constexpr std::size_t kMatrixWaiting = 4096;  // typed presses waiting at most

// The BW-2001 keyboard behind a MATRIXKB connector (P5q): a 10 x 8 key matrix and nothing else.
// Pins 1-10 ROW0-ROW9, 11-18 COL0-COL7, 19-20 not connected (pin index = pad - 1). Each key is a
// SwitchContact between its row and column Pins' Nets, joined by the Board's switch network as a
// switch's legs. Held from outside (setHeld) or typed in simulation time (type); no Faults.
class MatrixKeyboard final : public Component {
public:
    explicit MatrixKeyboard(std::string reference);
    static constexpr PinIndex kRow0 = 0, kColumn0 = 10;

    SwitchContact& contact(std::size_t key) { return contacts_[key]; }  // builder

    // Any thread; follow with Simulator or Runner wake(id()).
    void setHeld(std::size_t key, bool held) { held_[key].store(held, std::memory_order_relaxed); }
    bool held(std::size_t key) const { return held_[key].load(std::memory_order_relaxed); }
    // One producer thread. False (nothing queued) when `c` has no key or kMatrixWaiting presses
    // wait. Follow with a wake: an idle keyboard takes it at its next evaluation.
    bool type(char32_t c);
    void dropWaiting();
    // Any thread: the typed press held now (a key, and SHIFT with it), for drawing.
    bool scripted(
        std::size_t key) const;  // producer side, any time: the presses waiting now are skipped

    void powerOn(PowerOnContext& ctx) override;  // contacts open, Idle; waiting presses kept
    void evaluate(EvalContext& ctx) override;

private:
    enum class Phase : std::uint8_t { Idle, Hold, Gap };

    std::vector<SwitchContact> contacts_;
    std::array<std::atomic<bool>, kBw2001KeyCount> held_{};
    Phase phase_ = Phase::Idle;
    Time nextAt_ = 0;  // ps; outside Idle: when the phase's own wake is due
    MatrixPress current_{};
    std::atomic<std::uint32_t> showing_{kNotShowing};  // current_ while held: key | 0x8000 SHIFT
    static constexpr std::uint32_t kNotShowing = 0xFFFF'FFFF;
    std::vector<std::uint16_t> waiting_;            // key | 0x8000 for SHIFT
    std::atomic<std::uint32_t> head_{0}, tail_{0};  // SPSC: type() writes head_, evaluate tail_
    std::atomic<std::uint32_t> dropBefore_{0};
};

}  // namespace bw
