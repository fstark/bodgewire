#include "view/viewport.hpp"

#include <algorithm>
#include <cmath>

namespace bw {

void Viewport::fit(Bounds board, ScreenPoint panel) {
    board_ = board;
    panel_ = panel;
    const double width = static_cast<double>(board.max.x - board.min.x);
    const double height = static_cast<double>(board.max.y - board.min.y);
    if (width > 0 && height > 0 && panel.x > 0 && panel.y > 0) {
        nmToPixel_ = std::min(kFitMargin * std::min(panel.x / width, panel.y / height),
                              kMaxPixelsPerMm / 1e6);
        minNmToPixel_ = nmToPixel_;
    }
    const double cx = (static_cast<double>(board.min.x) + static_cast<double>(board.max.x)) / 2;
    const double cy = (static_cast<double>(board.min.y) + static_cast<double>(board.max.y)) / 2;
    double centreX = panel.x / 2;
    if (coveredLeft_ > 0 || coveredRight_ > 0) {
        const double half = width * nmToPixel_ / 2 + (1 - kFitMargin) / 2 * panel.x;
        const double wanted = (coveredLeft_ + panel.x - coveredRight_) / 2;
        centreX = half >= panel.x / 2 ? panel.x / 2 : std::clamp(wanted, half, panel.x - half);
    }
    offset_ = {centreX - cx * nmToPixel_, panel.y / 2 - cy * nmToPixel_};
    fitted_ = true;
}

void Viewport::resize(ScreenPoint panel) {
    if (fitted_) {
        fit(board_, panel);
        return;
    }
    const Point centre = toBoard({panel_.x / 2, panel_.y / 2});
    panel_ = panel;
    const ScreenPoint now = toScreen(centre);
    offset_ = {offset_.x + panel.x / 2 - now.x, offset_.y + panel.y / 2 - now.y};
}

void Viewport::zoomAt(ScreenPoint cursor, double notches) {
    const double bx = (cursor.x - offset_.x) / nmToPixel_;
    const double by = (cursor.y - offset_.y) / nmToPixel_;
    nmToPixel_ = std::clamp(nmToPixel_ * std::pow(kZoomStep, notches), minNmToPixel_,
                            std::max(minNmToPixel_, kMaxPixelsPerMm / 1e6));
    offset_ = {cursor.x - bx * nmToPixel_, cursor.y - by * nmToPixel_};
    fitted_ = nmToPixel_ == minNmToPixel_;
}

void Viewport::pan(ScreenPoint delta) {
    offset_ = {offset_.x + delta.x, offset_.y + delta.y};
    fitted_ = false;
}

void Viewport::frame(Bounds box, double share, double minPixelsPerMm) {
    const double width = static_cast<double>(box.max.x - box.min.x);
    const double height = static_cast<double>(box.max.y - box.min.y);
    double scale = nmToPixel_;
    if (width > 0 && height > 0 && panel_.x > 0 && panel_.y > 0) {
        scale = share * std::min(panel_.x / width, panel_.y / height);
    }
    nmToPixel_ = std::clamp(std::max(scale, minPixelsPerMm / 1e6), minNmToPixel_,
                            std::max(minNmToPixel_, kMaxPixelsPerMm / 1e6));
    const double cx = (static_cast<double>(box.min.x) + static_cast<double>(box.max.x)) / 2;
    const double cy = (static_cast<double>(box.min.y) + static_cast<double>(box.max.y)) / 2;
    offset_ = {panel_.x / 2 - cx * nmToPixel_, panel_.y / 2 - cy * nmToPixel_};
    fitted_ = false;
}

ScreenPoint Viewport::toScreen(Point p) const {
    return {static_cast<double>(p.x) * nmToPixel_ + offset_.x,
            static_cast<double>(p.y) * nmToPixel_ + offset_.y};
}

Point Viewport::toBoard(ScreenPoint s) const {
    return {std::llround((s.x - offset_.x) / nmToPixel_),
            std::llround((s.y - offset_.y) / nmToPixel_)};
}

}  // namespace bw
