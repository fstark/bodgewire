#pragma once

#include <cstddef>
#include <span>

namespace bw {

// One cap of a keyboard window (the BW-2001's chiclets, P5q; the BW-80's membrane keys, P5x),
// in key-widths (u) from the top left of the first row; every cap 1u high.
struct MatrixCap {
    std::size_t key;  // index in the keyboard's key table (Bw2001Keys, Zx80Keys)
    float x, y, width;
};

inline constexpr float kMatrixKeyboardWidth = 17.0f;  // u: main block 12, gap 1, pad 4
inline constexpr int kMatrixKeyboardRows = 5;
inline constexpr float kMatrixPadX = 13.0f;  // u: the numeric pad's first column

// The PET 2001's keyboard as photographed: the main block (`! " # $ % ' & \ ( ) ←`; RVS, Q-P, ↑;
// A-L, :, RETURN; SHIFT, Z-M, `, ; ?`, SHIFT; `@ [ ]`, SPACE, `< >`, STOP) and the numeric pad
// (HOME, CRSR ↓, CRSR →, DEL; 7 8 9 /; 4 5 6 *; 1 2 3 +; 0 . - =). Every key once.
std::span<const MatrixCap> Bw2001Caps();

// The ZX80's membrane as printed: four staggered rows of ten keys (1-0; Q-P; A-L, NEW LINE;
// SHIFT, Z-M, ., SPACE), each row 0, 1/2, 3/4 and 1/4 of a key to the right, rows kZx80RowPitch
// apart to leave room for the keyword above each key, the first row kZx80TopBand below the top for
// its words and arrows. A cap's x and width are its pitch (1u); the key printed in it is
// kZx80CapWidth wide, centred, and kZx80CapHeight high: wider than tall, as on the real one (P6l).
// Every key once.
inline constexpr float kZx80KeyboardWidth = 10.75f;  // u
inline constexpr float kZx80RowPitch = 1.03f;        // u
inline constexpr float kZx80TopBand = 0.38f;         // u: the words and arrows above the digits
inline constexpr float kZx80CapWidth = 0.80f;        // u
inline constexpr float kZx80CapHeight = 0.66f;       // u
inline constexpr float kZx80KeyboardHeight = kZx80TopBand + 3 * kZx80RowPitch + kZx80CapHeight;
std::span<const MatrixCap> Zx80Caps();

}  // namespace bw
