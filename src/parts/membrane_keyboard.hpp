#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

#include "parts/key_matrix.hpp"
#include "parts/press_queue.hpp"
#include "parts/zx80_keys.hpp"
#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// The ROM reads the keys once a frame (20 ms), takes a key read twice, and accepts a new one only
// after five frames that read no key (KEYBOARD's shift counter in RESULT_lo).
inline constexpr Time kMembraneHold = 60'000'000'000;  // ps: a typed key held 60 ms (3 frames)
inline constexpr Time kMembraneGap = 120'000'000'000;  // ps: then released 120 ms (6 frames)
inline constexpr Time kMembraneDelay = 5'000;          // ps: a diode and a contact

// The BW-80 keyboard behind a BW80KB connector (P5x): the ZX80's 40-key membrane, its rows
// the address lines A8-A15 (pads 1-8, read: the ROM pulls one low at a time during an IN) and
// its columns KB0-KB4 (pads 9-13, pulled up on the Board, 47k), each row reaching the keys
// through a diode as on the ZX80, so two keys on one column never join two address lines. A
// held key pulls its column low while its row is low (an open-collector output: nothing pulls
// it high). Held from outside (setHeld) or typed in simulation time (type); no Faults.
class MembraneKeyboard final : public Component, public KeyMatrix {
public:
    explicit MembraneKeyboard(std::string reference);
    static constexpr PinIndex kRow0 = 0, kColumn0 = 8;
    static constexpr int kRows = 8, kColumns = 5;

    KeyMatrixKind kind() const override { return KeyMatrixKind::Bw80; }
    std::size_t keyCount() const override { return kZx80KeyCount; }
    std::size_t shiftKey() const override { return 0; }  // row 0, column 0
    Component& component() override { return *this; }
    void setHeld(std::size_t key, bool held) override { held_[key] = held; }
    bool held(std::size_t key) const override { return held_[key]; }
    // Zx80Presses(text) queued; false when none was.
    bool type(std::u32string_view text) override;
    bool press(MatrixPress press) override {
        return press.key < kZx80KeyCount && queue_.push(press);
    }
    void dropWaiting() override { queue_.dropWaiting(); }
    bool scripted(std::size_t key) const override { return queue_.scripted(key, shiftKey()); }

    void powerOn(PowerOnContext& ctx) override;  // columns released, Idle; waiting presses kept
    void evaluate(EvalContext& ctx) override;

private:
    std::array<bool, kZx80KeyCount> held_{};
    PressQueue queue_{kMembraneHold, kMembraneGap};
};

}  // namespace bw
