#include "ui/tv_view.hpp"

#include <cstddef>

#include "sim/check.hpp"

namespace bw {

TvView::TvView(SDL_Renderer* renderer)
    : texture_(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                 kTvColumns, kTvRows)),
      pixels_(static_cast<std::size_t>(kTvColumns) * kTvRows * 4) {
    Check(texture_ != nullptr, "tv view: cannot create texture");
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
    show(TvPicture{});
}

TvView::~TvView() { SDL_DestroyTexture(texture_); }

void TvView::show(const TvPicture& picture) {
    cells_ = picture.frame.cells;
    if (picture.locked) {
        lines_ = picture.lines;  // the window keeps still until the next locked frame
    }
    upload();
}

void TvView::setTone(TvTone tone) {
    if (tone == tone_) {
        return;
    }
    tone_ = tone;
    upload();
}

void TvView::upload() {
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const std::uint8_t grey = Grey(cells_[i], tone_);
        pixels_[i * 4 + 0] = grey;
        pixels_[i * 4 + 1] = grey;
        pixels_[i * 4 + 2] = grey;
        pixels_[i * 4 + 3] = 255;
    }
    SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kTvColumns * 4);
}

}  // namespace bw
