#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "devices/tv.hpp"

namespace bw {

// The monitor's four knobs, left to right on its strip (P6f).
enum class TvKnob : std::uint8_t { HPos, VPos, Bright, Contrast };
inline constexpr std::array<TvKnob, 4> kTvKnobs{TvKnob::HPos, TvKnob::VPos, TvKnob::Bright,
                                                TvKnob::Contrast};
const char* Label(TvKnob knob);  // "H-POS", "V-POS", "BRIGHT", "CONTRAST"
double Mark(TvKnob knob);        // its starting turn: 0.5, 0.5, 0.25, 0.75

// The Tv frame's part the screen shows, in Tv cells (columns, rows): [x0, x1) × [y0, y1).
struct PictureWindow {
    float x0 = 0, x1 = kTvColumns, y0 = 0, y1 = kTvRows;
    bool operator==(const PictureWindow&) const = default;
};
// A real monitor's window: the line's first 10 µs (sync and back porch) and last 2 µs hidden,
// and the vertical retrace's lines (the BW-80's junk ends at row 27).
inline constexpr float kWindowLeft = 80, kWindowRight = 496, kWindowTop = 28;

// The picture's shift, fractions of the screen's width and height; right and down positive.
struct PictureShift {
    float x = 0, y = 0;
};
// The black level and gain a cell's level goes through.
struct TvTone {
    float black = 0, gain = 1;
    bool operator==(const TvTone&) const = default;
};

// The monitor's controls: the knobs' turns (0..1), its power switch and the hidden underscan
// trimmer, for the run.
class TvControls {
public:
    double turn(TvKnob knob) const { return turns_[static_cast<std::size_t>(knob)]; }
    void set(TvKnob knob, double turn);  // clamped to 0..1
    void reset(TvKnob knob) { set(knob, Mark(knob)); }
    bool on() const { return on_; }
    void setOn(bool on) { on_ = on; }
    bool underscan() const { return underscan_; }
    void setUnderscan(bool underscan) { underscan_ = underscan; }
    // Underscan: the whole frame. Else columns kWindowLeft..kWindowRight and rows kWindowTop to
    // the frame's last painted row: a frame of `lines` line starts paints rows 0..lines (row 0 the
    // part before the first), so y1 = lines + 1, at most kTvRows; kTvRows when `lines` is under
    // kWindowTop + 100 (no frame yet, or no sensible one).
    PictureWindow window(int lines) const;
    // H-POS and V-POS: (turn − 0.5) / 2 of the screen each way; clockwise right, and up.
    PictureShift shift() const;
    // CONTRAST: gain = turn / 0.75 (0..4/3); BRIGHT: black = (turn − 0.25) · 0.4 (−0.1..0.3).
    TvTone tone() const;

private:
    std::array<double, 4> turns_{0.5, 0.5, 0.25, 0.75};
    bool on_ = true;
    bool underscan_ = false;
};

// A cell's level (0 black .. 1 white, a Tv cell) as an 8-bit grey: lround(clamp(black + gain ·
// cell, 0, 1) · 255). At the marks, the grey TvView showed before P6f.
std::uint8_t Grey(float cell, TvTone tone);

// --tv: comma-separated `hpos:N%`, `vpos:N%`, `bright:N%`, `contrast:N%` (N 0..100, a decimal
// allowed), `off` and `underscan`, applied over `start`; nullopt on any bad item.
std::optional<TvControls> ParseTvSettings(std::string_view list, TvControls start = {});

}  // namespace bw
