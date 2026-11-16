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

#include "parts/key_matrix.hpp"
#include "parts/press_queue.hpp"
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

// The press that types `c` on the BW-2001 keyboard: 0x20-0x5F by the key whose legend it is ('_'
// the left-arrow key, '^' the up-arrow key), a-z as A-Z, '\n' or '\r' RETURN, '\b' or kKeyDel
// DEL, the kKey… named keys (CLR, INST, CRSR LF, CRSR UP with SHIFT); nullopt for anything else.
std::optional<MatrixPress> MatrixKeyFor(char32_t c);

// The BW-2001 keyboard behind a MATRIXKB connector (P5q): a 10 x 8 key matrix and nothing else.
// Pins 1-10 ROW0-ROW9, 11-18 COL0-COL7, 19-20 not connected (pin index = pad - 1). Each key is a
// SwitchContact between its row and column Pins' Nets, joined by the Board's switch network as a
// switch's legs. Held from outside (setHeld) or typed in simulation time (type); no Faults.
class MatrixKeyboard final : public Component, public KeyMatrix {
public:
    explicit MatrixKeyboard(std::string reference);
    static constexpr PinIndex kRow0 = 0, kColumn0 = 10;

    SwitchContact& contact(std::size_t key) { return contacts_[key]; }  // builder

    KeyMatrixKind kind() const override { return KeyMatrixKind::Bw2001; }
    std::size_t keyCount() const override { return kBw2001KeyCount; }
    std::size_t shiftKey() const override;  // the left SHIFT: row 8, column 0
    Component& component() override { return *this; }
    void setHeld(std::size_t key, bool held) override {
        held_[key].store(held, std::memory_order_relaxed);
    }
    bool held(std::size_t key) const override { return held_[key].load(std::memory_order_relaxed); }
    // Each character through MatrixKeyFor; false when none was queued.
    bool type(std::u32string_view text) override;
    // One character; false (nothing queued) when `c` has no key or kMatrixWaiting presses wait.
    bool type(char32_t c);
    void dropWaiting() override { queue_.dropWaiting(); }
    bool scripted(std::size_t key) const override { return queue_.scripted(key, shiftKey()); }

    void powerOn(PowerOnContext& ctx) override;  // contacts open, Idle; waiting presses kept
    void evaluate(EvalContext& ctx) override;

private:
    std::vector<SwitchContact> contacts_;
    std::array<std::atomic<bool>, kBw2001KeyCount> held_{};
    PressQueue queue_{kMatrixHold, kMatrixGap};
};

}  // namespace bw
