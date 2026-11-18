#pragma once

#include <filesystem>

#include <SDL3/SDL.h>

namespace bw {

class TvView;

// SDL GPU for every window bw_ui draws (P6j): the main one and the popped-out objects'. ImGui's
// backends own the popped-out windows' swapchains; this owns the device and the main window's
// frame.
class GpuRender {
public:
    explicit GpuRender(SDL_Window* main);  // the device claims `main`, VSYNC; error() if not
    ~GpuRender();                          // waits for the GPU, releases the device
    GpuRender(const GpuRender&) = delete;
    GpuRender& operator=(const GpuRender&) = delete;

    bool ready() const { return device_ != nullptr; }
    SDL_GPUDevice* device() const { return device_; }
    SDL_GPUTextureFormat format() const { return format_; }  // the main swapchain's
    // MAILBOX where the main window supports it (no tearing), else IMMEDIATE, else VSYNC.
    SDL_GPUPresentMode unpacedMode() const;

    // After ImGui::Render: the TV's upload, the main window cleared black and drawn, submitted;
    // then the popped-out windows (UpdatePlatformWindows, RenderPlatformWindowsDefault). Waits
    // for the windows' vsync.
    void present(TvView& tv);
    // After present: every visible viewport drawn again offscreen, read back and composed: with
    // only the main one, its framebuffer; else the desktop rectangle around them all at the main
    // viewport's scale on a dark grey backdrop. False with SDL's error.
    bool screenshot(const std::filesystem::path& path);

private:
    SDL_Window* main_;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
};

}  // namespace bw
