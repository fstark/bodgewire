#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace bw {

// What a cap of the ASCII keyboard does. Special: RETURN, BACK SPACE, ESC (one code each).
enum class CapKind : std::uint8_t { Character, Shift, Ctrl, Special };

// One cap of the bit-paired ASCII keyboard, in key-widths (u) from the top left of row 0.
struct KeyCap {
    float x, y, width;         // u; y is the row (0-4), every cap 1u high
    std::string_view label;    // plain legend ("1", "Q", "RETURN"); "" for the space bar
    std::string_view shifted;  // shifted legend ("!", "@"); "" when none
    CapKind kind;
    std::uint8_t code;  // Character, Special: the plain code; Shift, Ctrl: 0
};

inline constexpr float kKeyboardWidth = 14.0f;  // u
inline constexpr int kKeyboardRows = 5;

// Every cap, in reading order.
std::span<const KeyCap> KeyCaps();

// What `cap` sends with SHIFT and CTRL as given (bit-paired: a cap with a shifted legend flips
// bit 4; CTRL keeps the low 5 bits of 0x40-0x5F). nullopt for SHIFT, CTRL, and `0` shifted.
std::optional<std::uint8_t> CapCode(const KeyCap& cap, bool shift, bool ctrl);

// A cap and the modifiers it is pressed with.
struct CapPress {
    std::size_t cap;  // index in KeyCaps()
    bool shift = false, ctrl = false;
};

// The cap (and modifiers) that types `code`, with the fewest modifiers: a letter by its plain
// cap, 0x0D by RETURN rather than CTRL+M. nullopt above 0x5F.
std::optional<CapPress> CapFor(std::uint8_t code);

}  // namespace bw
