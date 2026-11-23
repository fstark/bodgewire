#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "sim/component.hpp"

namespace bw {

// Characters for named keys (Unicode private use), for the key maps and the PC keys: the PET's
// (P5q) and the ZX80's (P5x). RUBOUT is kKeyDel, NEW LINE is '\r'.
inline constexpr char32_t kKeyHome = 0xE000, kKeyClr = 0xE001, kKeyDown = 0xE002, kKeyUp = 0xE003,
                          kKeyRight = 0xE004, kKeyLeft = 0xE005, kKeyDel = 0xE006,
                          kKeyInst = 0xE007, kKeyStop = 0xE008, kKeyRvs = 0xE009, kKeyEdit = 0xE00A,
                          kKeyBreak = 0xE00B;

// A key and whether SHIFT is held with it.
struct MatrixPress {
    std::size_t key;
    bool shift = false;
};

// Which keyboard a KeyMatrix is: its look in the window and its key map.
enum class KeyMatrixKind : std::uint8_t { Bw2001, Bw80 };

// A keyboard of keys in a matrix, held from the UI or pressed from a queue in simulation time:
// the BW-2001's BW2001KB (P5q), the BW-80's BW80KB (P5x). Keys are indexed as the Part's
// key table lists them. Implemented beside Component.
class KeyMatrix {
public:
    virtual ~KeyMatrix() = default;
    virtual KeyMatrixKind kind() const = 0;
    virtual std::size_t keyCount() const = 0;
    virtual std::size_t shiftKey() const = 0;  // held with a shifted press
    virtual Component& component() = 0;        // for wakes

    // Simulation thread (Runner::post from the UI); follow with a wake(component().id()).
    virtual void setHeld(std::size_t key, bool held) = 0;
    virtual bool held(std::size_t key) const = 0;
    // One producer thread: the presses that type `text`, queued in simulation time. False when
    // nothing was queued (no key for any character, the queue full). Follow with a wake: an idle
    // keyboard takes it at its next evaluation.
    virtual bool type(std::u32string_view text) = 0;
    // One press queued as it is (SHIFT with a key: the PC's Shift with a letter on the BW-80);
    // false when nothing was queued (no such key, the queue full).
    virtual bool press(MatrixPress press) = 0;
    // Producer side, any time: the presses waiting now are skipped (power-off).
    virtual void dropWaiting() = 0;
    // Any thread: the scripted press held now includes `key` (SHIFT with a shifted press).
    virtual bool scripted(std::size_t key) const = 0;
};

}  // namespace bw
