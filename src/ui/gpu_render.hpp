#pragma once

#include <filesystem>

#include <SDL3/SDL.h>

namespace bw {

class TvView;

// SDL GPU for every window bw_ui draws (P6j): the main one and the popped-out objects'. ImGui's
// backends own the popped-out windows' swapchains; this owns the device and the main window's
// frame. The web build (gpu_render_web.cpp) draws the main window alone through SDL_Renderer
// (WebGL): no popped-out windows, no screenshot.
class GpuRender {
public:
    explicit GpuRender(SDL_Window* main);  // the device claims `main`, VSYNC; error() if not
    ~GpuRender();                          // waits for the GPU, releases the device
    GpuRender(const GpuRender&) = delete;
    GpuRender& operator=(const GpuRender&) = delete;

    bool ready() const;

    // ImGui's platform and renderer backends for a fresh context (multi-viewports on the
    // desktop), its frame, and their shutdown (the popped-out windows destroyed with it).
    void initImGui();
    void newFrame();
    void shutdownImGui();

    // After ImGui::Render: the TV's upload (none without one: the launcher), the main window
    // cleared black and drawn, submitted; then the popped-out windows (UpdatePlatformWindows,
    // RenderPlatformWindowsDefault). Waits for the windows' vsync (the web: the next animation
    // frame, the browser's turn).
    void present(TvView* tv);
    // After present: every visible viewport drawn again offscreen, read back and composed: with
    // only the main one, its framebuffer; else the desktop rectangle around them all at the main
    // viewport's scale on a dark grey backdrop. False with SDL's error.
    bool screenshot(const std::filesystem::path& path);

#ifdef __EMSCRIPTEN__
    SDL_Renderer* renderer() const { return renderer_; }

private:
    SDL_Window* main_;
    SDL_Renderer* renderer_ = nullptr;
#else
    SDL_GPUDevice* device() const { return device_; }
    SDL_GPUTextureFormat format() const { return format_; }  // the main swapchain's
    // MAILBOX where the main window supports it (no tearing), else IMMEDIATE, else VSYNC.
    SDL_GPUPresentMode unpacedMode() const;

private:
    SDL_Window* main_;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
#endif
};

}  // namespace bw
