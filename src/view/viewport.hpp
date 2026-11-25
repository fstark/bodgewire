#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "view/board_scene.hpp"

namespace bw {

// Panel coordinates: pixels from the panel's top-left, y down.
struct ScreenPoint {
    double x, y;
};

// Board nm ↔ panel pixels.
class Viewport {
public:
    static constexpr double kMaxPixelsPerMm = 100;  // first guess
    static constexpr double kZoomStep = 1.2;        // per wheel notch
    static constexpr double kFitMargin = 0.95;      // share of the panel the fitted Board fills

    void fit(Bounds board, ScreenPoint panel);  // whole outline, centred; sets the minimum zoom
    // Pixels at the panel's left and right that objects cover when the bench opens (the knob box,
    // the TV): a fit, its size unchanged, centres the Board across the width between them, moved
    // no closer than kFitMargin's gap to the panel's edges. Kept for later fits (resizes).
    void setCovered(double left, double right) {
        coveredLeft_ = left;
        coveredRight_ = right;
    }
    void resize(ScreenPoint panel);  // refits if fitted, else keeps the centre point
    void zoomAt(ScreenPoint cursor, double notches);  // the board point under the cursor stays
    void pan(ScreenPoint delta);
    // Centres `box` and zooms so it fills `share` of the panel, but at least `minPixelsPerMm`;
    // clamped to [fitted, kMaxPixelsPerMm].
    void frame(Bounds box, double share, double minPixelsPerMm);
    // As frame(box, share, 0), but never past `maxPixelsPerMm` (a chip framed so its pins are a
    // finger's size, no larger: P8f); still within [fitted, kMaxPixelsPerMm].
    void frameUpTo(Bounds box, double share, double maxPixelsPerMm);

    // The loupe's view (P8f): this one `factor` times larger, `at` at the centre of a `size`
    // panel; whatever the zoom's limits.
    Viewport magnified(Point at, double factor, ScreenPoint size) const;

    ScreenPoint toScreen(Point p) const;
    Point toBoard(ScreenPoint s) const;
    double toPixels(std::int64_t nm) const { return static_cast<double>(nm) * nmToPixel_; }
    double pixelsPerMm() const { return nmToPixel_ * 1e6; }
    bool fitted() const { return fitted_; }

private:
    double nmToPixel_ = 1e-5;  // px per nm
    double minNmToPixel_ = 0;  // fitted scale
    ScreenPoint offset_{};     // panel position of board (0, 0)
    Bounds board_{};
    ScreenPoint panel_{};
    bool fitted_ = true;  // still the fitted view: resize refits
    double coveredLeft_ = 0, coveredRight_ = 0;
};

}  // namespace bw
