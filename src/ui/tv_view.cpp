#include "ui/tv_view.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "sim/check.hpp"
#include "ui/gpu_render.hpp"

namespace bw {

namespace {

constexpr Uint32 kBytes = static_cast<Uint32>(kTvColumns) * kTvRows * 4;

#ifndef __EMSCRIPTEN__
SDL_GPUTexture* MakeTexture(SDL_GPUDevice* device) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = kTvColumns;
    info.height = kTvRows;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    return SDL_CreateGPUTexture(device, &info);
}

SDL_GPUTransferBuffer* MakeTransfer(SDL_GPUDevice* device) {
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = kBytes;
    return SDL_CreateGPUTransferBuffer(device, &info);
}
#endif

}  // namespace

#ifdef __EMSCRIPTEN__
TvView::TvView(const GpuRender& gpu)
    : texture_(SDL_CreateTexture(gpu.renderer(), SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STREAMING, kTvColumns, kTvRows)),
      pixels_(kBytes) {
    Check(texture_ != nullptr, "tv view: cannot create texture");
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
    show(TvPicture{});
}

TvView::~TvView() { SDL_DestroyTexture(texture_); }
#else
TvView::TvView(const GpuRender& gpu)
    : device_(gpu.device()), texture_(MakeTexture(device_)), transfer_(MakeTransfer(device_)),
      pixels_(kBytes) {
    Check(texture_ != nullptr && transfer_ != nullptr, "tv view: cannot create texture");
    show(TvPicture{});
}

TvView::~TvView() {
    SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    SDL_ReleaseGPUTexture(device_, texture_);
}
#endif

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
    dirty_ = true;
}

#ifdef __EMSCRIPTEN__
void TvView::upload() {
    if (dirty_ && SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kTvColumns * 4)) {
        dirty_ = false;
    }
}
#else
void TvView::upload(SDL_GPUCommandBuffer* commands) {
    if (!dirty_) {
        return;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_, true);
    if (mapped == nullptr) {
        return;  // tried again next frame
    }
    std::memcpy(mapped, pixels_.data(), kBytes);
    SDL_UnmapGPUTransferBuffer(device_, transfer_);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo from{};
    from.transfer_buffer = transfer_;
    SDL_GPUTextureRegion to{};
    to.texture = texture_;
    to.w = kTvColumns;
    to.h = kTvRows;
    to.d = 1;
    SDL_UploadToGPUTexture(copy, &from, &to, true);
    SDL_EndGPUCopyPass(copy);
    dirty_ = false;
}
#endif

}  // namespace bw
