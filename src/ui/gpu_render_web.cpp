// The web build's GpuRender (gpu_render.hpp): SDL GPU has no browser backend, so the main window
// is drawn by SDL_Renderer on WebGL, through ImGui's SDL_Renderer backend. No multi-viewports
// (no popped-out windows), no screenshot.
#include "ui/gpu_render.hpp"

#include <emscripten.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "ui/tv_view.hpp"

namespace bw {

namespace {

// The browser's turn: the canvas shown, events delivered, until the next animation frame. The
// frame loops never return to the browser otherwise (ASYNCIFY unwinds and resumes them here).
// clang-format off: JavaScript
EM_ASYNC_JS(void, WaitAnimationFrame, (), {
    await new Promise((resolve) => requestAnimationFrame(resolve));
});
// clang-format on

}  // namespace

GpuRender::GpuRender(SDL_Window* main) : main_(main) {
    // SDL would yield in its swap, through the renderer's indirect calls ASYNCIFY leaves alone
    // (ASYNCIFY_IGNORE_INDIRECT): present() yields instead.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
    renderer_ = SDL_CreateRenderer(main_, nullptr);
}

GpuRender::~GpuRender() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
}

bool GpuRender::ready() const { return renderer_ != nullptr; }

void GpuRender::initImGui() {
    ImGui_ImplSDL3_InitForSDLRenderer(main_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);
}

void GpuRender::newFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void GpuRender::shutdownImGui() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void GpuRender::present(TvView* tv) {
    if (tv != nullptr) {
        tv->upload();
    }
    ImDrawData* data = ImGui::GetDrawData();
    SDL_SetRenderScale(renderer_, data->FramebufferScale.x, data->FramebufferScale.y);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer3_RenderDrawData(data, renderer_);
    SDL_RenderPresent(renderer_);
    WaitAnimationFrame();
}

bool GpuRender::screenshot(const std::filesystem::path&) {
    return SDL_SetError("screenshot: not in the web build");
}

}  // namespace bw
