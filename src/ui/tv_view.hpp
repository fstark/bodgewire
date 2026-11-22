#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "runner/tv_consumer.hpp"
#include "view/tv_controls.hpp"

namespace bw {

class GpuRender;

// TvPicture → a GPU texture, drawn by the TV window (P6d), through the monitor's tone (P6f);
// uploaded in the frame's copy pass (P6j).
class TvView {
public:
    explicit TvView(const GpuRender& gpu);
    ~TvView();
    TvView(const TvView&) = delete;
    TvView& operator=(const TvView&) = delete;

    void show(const TvPicture& picture);  // keeps the frame, uploads it through the tone
    void clear();               // a blank frame, no lines: another Machine's picture to come (P7b)
    void setTone(TvTone tone);  // a change uploads the kept frame again
#ifdef __EMSCRIPTEN__
    void upload();  // what changed, before the frame's draws
#else
    void upload(SDL_GPUCommandBuffer* commands);  // what changed, before the frame's draws
#endif
    int lines() const { return lines_; }  // the last locked frame's line count, 0 before one
    ImTextureID texture() const {
        return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture_));
    }

private:
    void fill();

#ifdef __EMSCRIPTEN__
    SDL_Texture* texture_;
#else
    SDL_GPUDevice* device_;
    SDL_GPUTexture* texture_;
    SDL_GPUTransferBuffer* transfer_;
#endif
    std::vector<float> cells_;          // the shown frame
    std::vector<std::uint8_t> pixels_;  // RGBA
    TvTone tone_;
    int lines_ = 0;
    bool dirty_ = false;
};

}  // namespace bw
