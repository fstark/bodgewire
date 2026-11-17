#include "ui/tv_view.hpp"

#include <algorithm>
#include <cmath>
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
    for (std::size_t i = 0; i < picture.frame.cells.size(); ++i) {
        const auto grey = static_cast<std::uint8_t>(
            std::lround(std::clamp(picture.frame.cells[i], 0.0f, 1.0f) * 255.0f));
        pixels_[i * 4 + 0] = grey;
        pixels_[i * 4 + 1] = grey;
        pixels_[i * 4 + 2] = grey;
        pixels_[i * 4 + 3] = 255;
    }
    SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kTvColumns * 4);
}

}  // namespace bw
