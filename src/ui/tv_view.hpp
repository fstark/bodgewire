#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "runner/tv_consumer.hpp"
#include "view/tv_controls.hpp"

namespace bw {

// TvPicture → a GPU texture, drawn by the TV window (P6d), through the monitor's tone (P6f);
// uploaded in the frame's copy pass (P6j).
class TvView {
public:
    explicit TvView(SDL_GPUDevice* device);
    ~TvView();
    TvView(const TvView&) = delete;
    TvView& operator=(const TvView&) = delete;

    void show(const TvPicture& picture);          // keeps the frame, uploads it through the tone
    void setTone(TvTone tone);                    // a change uploads the kept frame again
    void upload(SDL_GPUCommandBuffer* commands);  // what changed, before the frame's draws
    int lines() const { return lines_; }  // the last locked frame's line count, 0 before one
    ImTextureID texture() const {
        return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture_));
    }

private:
    void fill();

    SDL_GPUDevice* device_;
    SDL_GPUTexture* texture_;
    SDL_GPUTransferBuffer* transfer_;
    std::vector<float> cells_;          // the shown frame
    std::vector<std::uint8_t> pixels_;  // RGBA
    TvTone tone_;
    int lines_ = 0;
    bool dirty_ = false;
};

}  // namespace bw
