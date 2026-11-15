#pragma once

#include <cstddef>
#include <span>

namespace bw {

// One chiclet of the BW-2001 keyboard window (P5q), in key-widths (u) from the top left of the
// main block's first row; every cap 1u high.
struct MatrixCap {
    std::size_t key;  // index in Bw2001Keys()
    float x, y, width;
};

inline constexpr float kMatrixKeyboardWidth = 17.0f;  // u: main block 12, gap 1, pad 4
inline constexpr int kMatrixKeyboardRows = 5;
inline constexpr float kMatrixPadX = 13.0f;  // u: the numeric pad's first column

// The PET 2001's keyboard as photographed: the main block (`! " # $ % ' & \ ( ) ←`; RVS, Q-P, ↑;
// A-L, :, RETURN; SHIFT, Z-M, `, ; ?`, SHIFT; `@ [ ]`, SPACE, `< >`, STOP) and the numeric pad
// (HOME, CRSR ↓, CRSR →, DEL; 7 8 9 /; 4 5 6 *; 1 2 3 +; 0 . - =). Every key once.
std::span<const MatrixCap> Bw2001Caps();

}  // namespace bw
