#pragma once

#include <span>
#include <vector>

namespace bw {

// One item on the shelf, for its place: its tray, its width in cells, and whether it sits in the
// tray at the right end (the Machine's) and is set apart in it (the Machine slot).
struct ShelfCell {
    int tray = 0;
    float cells = 1;
    bool right = false;
    bool apart = false;
};

struct ShelfMetrics {
    float cell = 0;      // a square cell's side, px
    float cellGap = 0;   // between cells of a tray
    float groupGap = 0;  // between trays, and half of it at the shelf's ends
};

struct ShelfPlaces {
    std::vector<float> x;  // each item's left edge, px from the shelf's left
    float leftEnd = 0;     // where the left-hand trays end: the hint starts after it
    float rightStart = 0;  // where the right-hand tray starts
};

// The shelf's layout (P6c, P6o): the trays from the left edge in order, a tray's gap between
// trays; the right-hand tray from the right edge, its set-apart item half a tray's gap away.
ShelfPlaces LayOutShelf(std::span<const ShelfCell> items, float width, const ShelfMetrics& metrics);

// The width the shelf needs: its left-hand trays, a tray's gap, its right-hand tray.
float ShelfWidth(std::span<const ShelfCell> items, const ShelfMetrics& metrics);
// The Shelf's metrics, and the narrow ones for a window they do not fit (P8f: a tablet's 1080 px,
// where the BW-2001's two sides would overlap).
inline constexpr ShelfMetrics kShelfMetrics{48, 4, 20};
inline constexpr ShelfMetrics kShelfNarrow{44, 4, 12};
const ShelfMetrics& ShelfMetricsFor(std::span<const ShelfCell> items, float width);

}  // namespace bw
