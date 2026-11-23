#pragma once

#include <filesystem>
#include <memory>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace bw {

class TvView;

// What draws bw_ui's windows (P6j), one implementation a build, each with its own Backend: on the
// desktop SDL GPU (gpu_render.cpp) for the main window and the popped-out objects' (ImGui's
// backends own their swapchains); on the web SDL_Renderer on WebGL (gpu_render_web.cpp, P8b) for
// the main window alone: no popped-out windows, no screenshot. Either owns the TV's texture.
class GpuRender {
public:
    static SDL_WindowFlags windowFlags();  // for the main window, made before this
    explicit GpuRender(SDL_Window* main);  // claims `main`, VSYNC; !ready() with SDL's error
    ~GpuRender();                          // waits for the GPU, releases everything
    GpuRender(const GpuRender&) = delete;
    GpuRender& operator=(const GpuRender&) = delete;

    bool ready() const;

    // ImGui's platform and renderer backends for a fresh context (multi-viewports on the
    // desktop), its frame, and their shutdown (the popped-out windows destroyed with it).
    void initImGui();
    void newFrame();
    void shutdownImGui();

    // The TV's texture, kTvColumns × kTvRows RGBA drawn smooth: one, for the screen's one TvView.
    ImTextureID tvTexture() const;

    // After ImGui::Render: the TV's pixels uploaded if they changed (none without a TV), the main
    // window cleared black and drawn, submitted; then the popped-out windows
    // (UpdatePlatformWindows, RenderPlatformWindowsDefault). Waits for the windows' vsync (the
    // web: the next animation frame, the browser's turn).
    void present(TvView* tv);
    // After present: every visible viewport drawn again offscreen, read back and composed: with
    // only the main one, its framebuffer; else the desktop rectangle around them all at the main
    // viewport's scale on a dark grey backdrop. False with SDL's error.
    bool screenshot(const std::filesystem::path& path);

private:
    struct Backend;  // defined by the build's implementation
    std::unique_ptr<Backend> backend_;
};

}  // namespace bw
