#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace bw {

// The number pad a number's field opens (P8f): no on-screen keyboard comes up in a browser, and
// every field of the UI holds a number. Its keys and what each does to the text; drawn by
// ui/controls.cpp.
enum class PadKind : std::uint8_t {
    Hex,       // an address: 0-9, A-F
    Pattern,   // a bus Trigger: hex, x for any digit
    Duration,  // a time: digits, a point, then ns, us or ms
};

inline constexpr std::size_t kPadColumns = 4;
inline constexpr std::string_view kPadBackspace = "<", kPadConfirm = "OK";

// The pad's keys row by row, kPadColumns a row; "" leaves a cell empty. kPadBackspace takes a
// character away, kPadConfirm confirms, a unit ("ns", "us", "ms") is put after the number and
// confirms.
std::span<const std::string_view> PadKeys(PadKind kind);
bool IsPadUnit(std::string_view key);

// `text` after `key`; unchanged when the key does not fit: past `limit` characters, a second
// point, a character the kind has not. `fresh`: nothing typed yet, `text` is the field's value as
// it was shown: a character replaces it, backspace empties it, a unit keeps its number.
std::string PadTyped(std::string text, std::string_view key, PadKind kind, std::size_t limit,
                     bool fresh);

}  // namespace bw
