#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "parts/key_matrix.hpp"

namespace bw {

// One key of the BW-80 keyboard (the ZX80's, P5x): where it is in the matrix and what its cap
// says. Rows are the address lines A8-A15 the ROM pulls low one at a time; columns are the five
// lines KB0-KB4 it reads on D0-D4.
struct Zx80Key {
    std::uint8_t row;          // 0-7: A8-A15
    std::uint8_t column;       // 0-4: KB0-KB4
    std::string_view legend;   // "A", "NEW LINE", "SPACE", "SHIFT"
    std::string_view shifted;  // "" or the yellow legend: ":", "THEN", "EDIT", "GRAPHIC", an arrow
    std::string_view keyword;  // "" or the keyword above the key, typed by it in K mode: "LIST"
};

// The ZX80's 40 keys, row 0 column 0 first (SHIFT Z X C V; A-G; Q-T; 1-5; 0 9 8 7 6; P O I U Y;
// NEW LINE L K J H; SPACE . M N B); the index of a key in this span is its index everywhere.
std::span<const Zx80Key> Zx80Keys();
std::optional<std::size_t> Zx80KeyAt(int row, int column);
inline constexpr std::size_t kZx80KeyCount = 40;
inline constexpr char32_t kPoundSign = 0xA3;

// The press that types `c`: letters (either case) and digits by their key; ' ', '.', '\n' or
// '\r' (NEW LINE); the shifted symbols with SHIFT (" $ ( ) * - + = : ; ? / < > , and the pound
// sign); '\b' and kKeyDel (RUBOUT), kKeyUp, Down, Left, Right, Home and Edit with SHIFT;
// kKeyBreak is SPACE alone (the ROM reads it without SHIFT while a program runs); nullopt for
// anything else.
std::optional<MatrixPress> Zx80KeyFor(char32_t c);

// The presses that type `text` as the ZX80's editor reads it: a keyword where the cursor is K
// (the start of a line, after its number; after THEN) is its key alone; THEN, TO, AND, OR, NOT
// and ** are their SHIFT keys anywhere; every other character goes through Zx80KeyFor, those
// with no key skipped. GOTO and GO TO, GOSUB and GO SUB, CONT and CONTINUE, RAND and RANDOMISE,
// RET and RETURN are the same keyword. Outside a string a space is typed only between two plain
// words (the editor spaces keywords itself); inside one every character is typed as it is.
std::vector<MatrixPress> Zx80Presses(std::u32string_view text);

}  // namespace bw
