#pragma once

#include <cstdint>

namespace bw {

// The objects bw_ui places on the bench (P6d): the equipment, and the brief and Debug windows.
enum class BenchObject : std::uint8_t {
    Tv,
    Keyboard,
    Knob,
    Analyzer,
    Scope,
    Meter,
    Manual,
    Brief,
    Debug
};

struct Extent {
    float width = 0, height = 0;
};

// The window's pivot (0..1 of its size) goes to (x, y): pivot (1, 0) puts its top-right corner
// there. Logical px in the bench area (the work area above the shelf), origin at its top left.
struct Place {
    float x = 0, y = 0, pivotX = 0, pivotY = 0;
};

inline constexpr float kBenchMargin = 16;
// The TV case's proportions, fractions of its width: a bezel each side and on top, a 4:3
// picture, a chin below.
inline constexpr float kTvBezel = 0.06f;
inline constexpr float kTvChin = 0.12f;
inline constexpr float kTvPicture = 1 - 2 * kTvBezel;                        // its width
inline constexpr float kTvAspect = kTvBezel + kTvPicture * 0.75f + kTvChin;  // height / width
inline constexpr float kTvFirstWidth = 440;
inline constexpr float kKeyboardFirstWidth = 640;
inline constexpr Extent kKnobBox{140, 160};  // one paddle controller

// Where an object comes out, every time (UX_DESIGN: muscle memory).
Place DefaultPlace(BenchObject object, Extent bench);
// The first size of an object that is not an instrument: the TV's and the knob's (`count`
// paddles side by side) from their constants; the keyboard kKeyboardFirstWidth wide, narrowed
// until it fits between its place and the bench's bottom margin (`aspect`: height / width); the
// manuals, brief and Debug windows' fixed sizes.
Extent FirstSize(BenchObject object, Extent bench, float aspect = 1, int count = 1);
// The instruments' two presets (Analyzer, Scope, Meter); the first size is the normal one.
Extent NormalSize(BenchObject instrument);
Extent MinimalSize(BenchObject instrument);
// The size button: the preset further from `current` (by the two sides' distances summed).
Extent NextPreset(Extent current, Extent minimal, Extent normal);

}  // namespace bw
