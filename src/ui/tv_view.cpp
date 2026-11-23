#include "ui/tv_view.hpp"

#include <algorithm>
#include <cstddef>

#include "devices/tv.hpp"

namespace bw {

TvView::TvView(ImTextureID texture)
    : texture_(texture), pixels_(static_cast<std::size_t>(kTvColumns) * kTvRows * 4) {
    show(TvPicture{});
}

void TvView::show(const TvPicture& picture) {
    cells_ = picture.frame.cells;
    if (picture.locked) {
        lines_ = picture.lines;  // the window keeps still until the next locked frame
    }
    fill();
}

void TvView::clear() {
    std::ranges::fill(cells_, 0.0f);
    lines_ = 0;
    fill();
}

void TvView::setTone(TvTone tone) {
    if (tone == tone_) {
        return;
    }
    tone_ = tone;
    fill();
}

void TvView::fill() {
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const std::uint8_t grey = Grey(cells_[i], tone_);
        pixels_[i * 4 + 0] = grey;
        pixels_[i * 4 + 1] = grey;
        pixels_[i * 4 + 2] = grey;
        pixels_[i * 4 + 3] = 255;
    }
    changed_ = true;
}

}  // namespace bw
