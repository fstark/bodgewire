// The web build's GpuRender (gpu_render.hpp): SDL GPU has no browser backend, so the main window
// is drawn by SDL_Renderer on WebGL, through ImGui's SDL_Renderer backend. No multi-viewports
// (no popped-out windows), no screenshot.
#include "ui/gpu_render.hpp"

#include <cstdint>
#include <memory>

#include <emscripten.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "devices/tv.hpp"
#include "ui/tv_view.hpp"

namespace bw {

namespace {

// The browser's turn: the canvas shown, events delivered, until the next animation frame. The
// frame loops never return to the browser otherwise (ASYNCIFY unwinds and resumes them here).
// ASYNCIFY cannot unwind through a live try block under wasm exceptions (the link's
// "not compatible" warning): no try may enclose a frame loop, or the page traps here.
// clang-format off: JavaScript
EM_ASYNC_JS(void, WaitAnimationFrame, (), {
    await new Promise((resolve) => requestAnimationFrame(resolve));
});
// clang-format on

}  // namespace

struct GpuRender::Backend {
    SDL_Window* main = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* tv = nullptr;
};

SDL_WindowFlags GpuRender::windowFlags() {
    // The whole page, following the browser window's size; OpenGL (WebGL) for SDL_Renderer from
    // the start: it would recreate the window, and SDL's page juggling fails on the way.
    return SDL_WINDOW_FILL_DOCUMENT | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
           SDL_WINDOW_HIGH_PIXEL_DENSITY;
}

GpuRender::GpuRender(SDL_Window* main) : backend_(std::make_unique<Backend>()) {
    Backend& b = *backend_;
    b.main = main;
    // SDL would yield in its swap, through the renderer's indirect calls ASYNCIFY leaves alone
    // (ASYNCIFY_IGNORE_INDIRECT): present() yields instead.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
    b.renderer = SDL_CreateRenderer(b.main, nullptr);
    if (b.renderer == nullptr) {
        return;
    }
    b.tv = SDL_CreateTexture(b.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                             kTvColumns, kTvRows);
    if (b.tv != nullptr) {
        SDL_SetTextureScaleMode(b.tv, SDL_SCALEMODE_LINEAR);
    }
}

GpuRender::~GpuRender() {
    Backend& b = *backend_;
    if (b.tv != nullptr) {
        SDL_DestroyTexture(b.tv);
    }
    if (b.renderer != nullptr) {
        SDL_DestroyRenderer(b.renderer);
    }
}

bool GpuRender::ready() const { return backend_->renderer != nullptr && backend_->tv != nullptr; }

void GpuRender::initImGui() {
    ImGui_ImplSDL3_InitForSDLRenderer(backend_->main, backend_->renderer);
    ImGui_ImplSDLRenderer3_Init(backend_->renderer);
}

void GpuRender::newFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void GpuRender::shutdownImGui() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

ImTextureID GpuRender::tvTexture() const {
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(backend_->tv));
}

void GpuRender::present(TvView* tv) {
    const Backend& b = *backend_;
    if (tv != nullptr && tv->changed() &&
        SDL_UpdateTexture(b.tv, nullptr, tv->pixels().data(), kTvColumns * 4)) {
        tv->uploaded();
    }
    ImDrawData* data = ImGui::GetDrawData();
    SDL_SetRenderScale(b.renderer, data->FramebufferScale.x, data->FramebufferScale.y);
    SDL_SetRenderDrawColor(b.renderer, 0, 0, 0, 255);
    SDL_RenderClear(b.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(data, b.renderer);
    SDL_RenderPresent(b.renderer);
    WaitAnimationFrame();
}

bool GpuRender::screenshot(const std::filesystem::path&) {
    return SDL_SetError("screenshot: not in the web build");
}

}  // namespace bw
