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
    void resize(ScreenPoint panel);             // refits if fitted, else keeps the centre point
    void zoomAt(ScreenPoint cursor, double notches);  // the board point under the cursor stays
    void pan(ScreenPoint delta);

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
};

}  // namespace bw
