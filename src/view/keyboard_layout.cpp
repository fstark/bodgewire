#include "view/keyboard_layout.hpp"

#include <array>
#include <utility>

#include "parts/keyboard.hpp"

namespace bw {

namespace {

using enum CapKind;

constexpr KeyCap Char(float x, float y, std::string_view label, std::string_view shifted = "") {
    return {x, y, 1.0f, label, shifted, Character, static_cast<std::uint8_t>(label[0])};
}

// Layout: docs/p4o-keyboard-window/KEYBOARD_WINDOW_DESIGN.md §3.
constexpr std::array kCaps{
    // Row 0, centred: digits and the punctuation paired with them.
    Char(1, 0, "1", "!"),
    Char(2, 0, "2", "\""),
    Char(3, 0, "3", "#"),
    Char(4, 0, "4", "$"),
    Char(5, 0, "5", "%"),
    Char(6, 0, "6", "&"),
    Char(7, 0, "7", "'"),
    Char(8, 0, "8", "("),
    Char(9, 0, "9", ")"),
    Char(10, 0, "0"),
    Char(11, 0, ":", "*"),
    Char(12, 0, "-", "="),
    // Row 1.
    KeyCap{0, 1, 1.5f, "ESC", "", Special, 0x1B},
    Char(1.5f, 1, "Q"),
    Char(2.5f, 1, "W"),
    Char(3.5f, 1, "E"),
    Char(4.5f, 1, "R"),
    Char(5.5f, 1, "T"),
    Char(6.5f, 1, "Y"),
    Char(7.5f, 1, "U"),
    Char(8.5f, 1, "I"),
    Char(9.5f, 1, "O", "_"),
    Char(10.5f, 1, "P", "@"),
    KeyCap{11.5f, 1, 2.5f, "BACK SPACE", "", Special, 0x08},
    // Row 2.
    KeyCap{0, 2, 1.75f, "CTRL", "", Ctrl, 0},
    Char(1.75f, 2, "A"),
    Char(2.75f, 2, "S"),
    Char(3.75f, 2, "D"),
    Char(4.75f, 2, "F"),
    Char(5.75f, 2, "G"),
    Char(6.75f, 2, "H"),
    Char(7.75f, 2, "J"),
    Char(8.75f, 2, "K", "["),
    Char(9.75f, 2, "L", "\\"),
    Char(10.75f, 2, ";", "+"),
    KeyCap{11.75f, 2, 2.25f, "RETURN", "", Special, 0x0D},
    // Row 3.
    KeyCap{0, 3, 2.25f, "SHIFT", "", Shift, 0},
    Char(2.25f, 3, "Z"),
    Char(3.25f, 3, "X"),
    Char(4.25f, 3, "C"),
    Char(5.25f, 3, "V"),
    Char(6.25f, 3, "B"),
    Char(7.25f, 3, "N", "^"),
    Char(8.25f, 3, "M", "]"),
    Char(9.25f, 3, ",", "<"),
    Char(10.25f, 3, ".", ">"),
    Char(11.25f, 3, "/", "?"),
    KeyCap{12.25f, 3, 1.75f, "SHIFT", "", Shift, 0},
    // Row 4.
    KeyCap{3, 4, 8, "", "", Character, 0x20},
};

bool IsLetter(std::uint8_t code) { return code >= 'A' && code <= 'Z'; }

std::array<std::optional<CapPress>, kKeyboardLastCode + 1> BuildCapFor() {
    std::array<std::optional<CapPress>, kKeyboardLastCode + 1> table{};
    // Fewest modifiers first: none, SHIFT, CTRL, both.
    for (const auto [shift, ctrl] :
         {std::pair{false, false}, {true, false}, {false, true}, {true, true}}) {
        for (std::size_t i = 0; i < kCaps.size(); ++i) {
            if (const auto code = CapCode(kCaps[i], shift, ctrl); code && !table[*code]) {
                table[*code] = CapPress{i, shift, ctrl};
            }
        }
    }
    return table;
}

}  // namespace

std::span<const KeyCap> KeyCaps() { return kCaps; }

std::optional<std::uint8_t> CapCode(const KeyCap& cap, bool shift, bool ctrl) {
    if (cap.kind == Shift || cap.kind == Ctrl) {
        return std::nullopt;
    }
    if (cap.kind == Special) {
        return cap.code;
    }
    std::uint8_t code = cap.code;
    if (shift) {
        if (!cap.shifted.empty()) {
            code ^= 0x10;
        } else if (!IsLetter(code) && code != ' ') {
            return std::nullopt;  // `0`: nothing shifted, as on the originals
        }
    }
    if (ctrl && code >= 0x40 && code <= kKeyboardLastCode) {
        code &= 0x1F;
    }
    return code;
}

std::optional<CapPress> CapFor(std::uint8_t code) {
    static const auto table = BuildCapFor();
    return code < table.size() ? table[code] : std::nullopt;
}

}  // namespace bw
