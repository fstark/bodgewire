#include "ui/gpu_render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <imgui_internal.h>

#include "devices/tv.hpp"
#include "ui/tv_view.hpp"

namespace bw {

namespace {

constexpr SDL_FColor kClear{0, 0, 0, 1};
constexpr std::uint8_t kBackdrop = 40;  // the composed screenshot's desktop
constexpr Uint32 kTvBytes = static_cast<Uint32>(kTvColumns) * kTvRows * 4;

// ImGui's draw data recorded into `target` (cleared first) in its own render pass.
void Draw(ImDrawData* data, SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target) {
    ImGui_ImplSDLGPU3_PrepareDrawData(data, commands);
    SDL_GPUColorTargetInfo info{};
    info.texture = target;
    info.clear_color = kClear;
    info.load_op = SDL_GPU_LOADOP_CLEAR;
    info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &info, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(data, commands, pass);
    SDL_EndGPURenderPass(pass);
}

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const { SDL_DestroySurface(surface); }
};
using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

// One viewport drawn offscreen at its framebuffer size and read back; null with SDL's error.
Surface ReadBack(SDL_GPUDevice* device, SDL_GPUTextureFormat format, SDL_PixelFormat pixels,
                 ImDrawData* data) {
    const auto width =
        static_cast<Uint32>(std::lround(data->DisplaySize.x * data->FramebufferScale.x));
    const auto height =
        static_cast<Uint32>(std::lround(data->DisplaySize.y * data->FramebufferScale.y));
    if (width == 0 || height == 0) {
        SDL_SetError("empty viewport");
        return nullptr;
    }
    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = format;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);
    const Uint32 bytes = width * height * 4;
    SDL_GPUTransferBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    bufferInfo.size = bytes;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &bufferInfo);
    Surface surface;
    if (texture != nullptr && buffer != nullptr) {
        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
        Draw(data, commands, texture);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion region{};
        region.texture = texture;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_GPUTextureTransferInfo transfer{};
        transfer.transfer_buffer = buffer;
        SDL_DownloadFromGPUTexture(copy, &region, &transfer);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        if (fence != nullptr) {
            SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
            surface.reset(
                SDL_CreateSurface(static_cast<int>(width), static_cast<int>(height), pixels));
            const auto* source =
                static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, buffer, false));
            if (surface && source != nullptr) {
                for (Uint32 row = 0; row < height; ++row) {
                    std::memcpy(static_cast<std::uint8_t*>(surface->pixels) +
                                    static_cast<std::size_t>(row) * surface->pitch,
                                source + static_cast<std::size_t>(row) * width * 4, width * 4);
                }
                // Opaque: the clear's alpha, but a translucent draw over it may leave less.
                SDL_SetSurfaceBlendMode(surface.get(), SDL_BLENDMODE_NONE);
            }
            if (source != nullptr) {
                SDL_UnmapGPUTransferBuffer(device, buffer);
            }
        }
    }
    if (buffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, buffer);
    }
    if (texture != nullptr) {
        SDL_ReleaseGPUTexture(device, texture);
    }
    return surface;
}

// The TV's texture and the buffer its pixels go through, for `device`.
SDL_GPUTexture* MakeTvTexture(SDL_GPUDevice* device) {
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

SDL_GPUTransferBuffer* MakeTvTransfer(SDL_GPUDevice* device) {
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = kTvBytes;
    return SDL_CreateGPUTransferBuffer(device, &info);
}

// MAILBOX where `window` supports it (no tearing), else IMMEDIATE, else VSYNC.
SDL_GPUPresentMode UnpacedMode(SDL_GPUDevice* device, SDL_Window* window) {
    for (const SDL_GPUPresentMode mode :
         {SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE}) {
        if (SDL_WindowSupportsGPUPresentMode(device, window, mode)) {
            return mode;
        }
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

}  // namespace

struct GpuRender::Backend {
    SDL_Window* main = nullptr;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;  // the main swapchain's
    SDL_GPUTexture* tv = nullptr;
    SDL_GPUTransferBuffer* tvTransfer = nullptr;

    // The TV's pixels into its texture, in a copy pass of `commands`; false if the buffer
    // would not map (tried again next frame).
    bool uploadTv(SDL_GPUCommandBuffer* commands, const TvView& view) const {
        void* mapped = SDL_MapGPUTransferBuffer(device, tvTransfer, true);
        if (mapped == nullptr) {
            return false;
        }
        std::memcpy(mapped, view.pixels().data(), kTvBytes);
        SDL_UnmapGPUTransferBuffer(device, tvTransfer);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureTransferInfo from{};
        from.transfer_buffer = tvTransfer;
        SDL_GPUTextureRegion to{};
        to.texture = tv;
        to.w = kTvColumns;
        to.h = kTvRows;
        to.d = 1;
        SDL_UploadToGPUTexture(copy, &from, &to, true);
        SDL_EndGPUCopyPass(copy);
        return true;
    }
};

SDL_WindowFlags GpuRender::windowFlags() {
    return SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
}

GpuRender::GpuRender(SDL_Window* main) : backend_(std::make_unique<Backend>()) {
    Backend& b = *backend_;
    b.main = main;
    b.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                       SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
                                   false, nullptr);
    if (b.device == nullptr) {
        return;
    }
    if (!SDL_ClaimWindowForGPUDevice(b.device, b.main)) {
        SDL_DestroyGPUDevice(b.device);
        b.device = nullptr;
        return;
    }
    SDL_SetGPUSwapchainParameters(b.device, b.main, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);
    b.format = SDL_GetGPUSwapchainTextureFormat(b.device, b.main);
    b.tv = MakeTvTexture(b.device);
    b.tvTransfer = MakeTvTransfer(b.device);
}

GpuRender::~GpuRender() {
    Backend& b = *backend_;
    if (b.device == nullptr) {
        return;
    }
    SDL_WaitForGPUIdle(b.device);
    if (b.tvTransfer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(b.device, b.tvTransfer);
    }
    if (b.tv != nullptr) {
        SDL_ReleaseGPUTexture(b.device, b.tv);
    }
    SDL_ReleaseWindowFromGPUDevice(b.device, b.main);
    SDL_DestroyGPUDevice(b.device);
}

bool GpuRender::ready() const {
    return backend_->device != nullptr && backend_->tv != nullptr &&
           backend_->tvTransfer != nullptr;
}

void GpuRender::initImGui() {
    const Backend& b = *backend_;
    // Multi-viewports, docking off (P6j): a popped-out object in its own desktop window, not
    // tied to the main one (ImGui's default), with no title bar.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // NoDefaultParent: true
    ImGui_ImplSDL3_InitForSDLGPU(b.main);
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = b.device;
    info.ColorTargetFormat = b.format;
    // The popped-out windows never wait for their display's vsync: one on a slower display (a
    // 30 Hz monitor) would pace the whole UI at its rate. The main window's vsync paces it.
    info.PresentMode = UnpacedMode(b.device, b.main);
    ImGui_ImplSDLGPU3_Init(&info);
}

void GpuRender::newFrame() {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void GpuRender::shutdownImGui() {
    SDL_WaitForGPUIdle(backend_->device);
    ImGui_ImplSDL3_Shutdown();  // the popped-out windows destroyed with it
    ImGui_ImplSDLGPU3_Shutdown();
}

ImTextureID GpuRender::tvTexture() const {
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(backend_->tv));
}

void GpuRender::present(TvView* tv) {
    const Backend& b = *backend_;
    ImDrawData* data = ImGui::GetDrawData();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(b.device);
    if (tv != nullptr && tv->changed() && b.uploadTv(commands, *tv)) {
        tv->uploaded();
    }
    SDL_GPUTexture* swapchain = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(commands, b.main, &swapchain, nullptr, nullptr);
    if (swapchain != nullptr && data->DisplaySize.x > 0 && data->DisplaySize.y > 0) {
        Draw(data, commands, swapchain);
    }
    SDL_SubmitGPUCommandBuffer(commands);
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool GpuRender::screenshot(const std::filesystem::path& path) {
    const Backend& b = *backend_;
    SDL_PixelFormat pixels = SDL_PIXELFORMAT_UNKNOWN;
    if (b.format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
        pixels = SDL_PIXELFORMAT_BGRA32;
    } else if (b.format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
        pixels = SDL_PIXELFORMAT_RGBA32;
    } else {
        return SDL_SetError("screenshot: swapchain format %d", static_cast<int>(b.format));
    }
    // ImGui::Render set every viewport's draw data; present() has made their windows.
    struct Shot {
        Surface surface;
        ImVec2 at, size;
    };
    std::vector<Shot> shots;
    // Back to front: the main window, then the others by when they were last focused (ImGui's
    // own fallback z-order, IsViewportAbove), a window never focused in the order it came.
    std::vector<ImGuiViewportP*> order;
    for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports) {
        if (viewport != ImGui::GetMainViewport()) {
            order.push_back(static_cast<ImGuiViewportP*>(viewport));
        }
    }
    std::ranges::stable_sort(order, {}, &ImGuiViewportP::LastFocusedStampCount);
    order.insert(order.begin(), static_cast<ImGuiViewportP*>(ImGui::GetMainViewport()));
    SDL_WaitForGPUIdle(b.device);
    for (ImGuiViewportP* viewport : order) {
        ImDrawData* data = viewport->DrawData;
        if (data == nullptr || !data->Valid || data->DisplaySize.x <= 0 ||
            (viewport->Flags & ImGuiViewportFlags_IsMinimized) != 0) {
            continue;
        }
        Surface surface = ReadBack(b.device, b.format, pixels, data);
        if (!surface) {
            return false;
        }
        shots.push_back({std::move(surface), viewport->Pos, viewport->Size});
    }
    SDL_WaitForGPUIdle(b.device);
    if (shots.empty()) {
        return SDL_SetError("screenshot: nothing drawn");
    }
    if (shots.size() == 1) {
        return SDL_SavePNG(shots.front().surface.get(), path.string().c_str());
    }
    ImVec2 min = shots.front().at, max = min;
    for (const Shot& shot : shots) {
        min = {std::min(min.x, shot.at.x), std::min(min.y, shot.at.y)};
        max = {std::max(max.x, shot.at.x + shot.size.x), std::max(max.y, shot.at.y + shot.size.y)};
    }
    const float scale = ImGui::GetMainViewport()->DrawData->FramebufferScale.x;
    const auto px = [scale](float v) { return static_cast<int>(std::lround(v * scale)); };
    const Surface canvas(SDL_CreateSurface(px(max.x - min.x), px(max.y - min.y), pixels));
    if (!canvas) {
        return false;
    }
    SDL_FillSurfaceRect(canvas.get(), nullptr,
                        SDL_MapSurfaceRGB(canvas.get(), kBackdrop, kBackdrop, kBackdrop));
    for (const Shot& shot : shots) {
        const SDL_Rect to{px(shot.at.x - min.x), px(shot.at.y - min.y), px(shot.size.x),
                          px(shot.size.y)};
        if (!SDL_BlitSurfaceScaled(shot.surface.get(), nullptr, canvas.get(), &to,
                                   SDL_SCALEMODE_LINEAR)) {
            return false;
        }
    }
    return SDL_SavePNG(canvas.get(), path.string().c_str());
}

}  // namespace bw
