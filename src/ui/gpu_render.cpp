#include "ui/gpu_render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>
#include <imgui_internal.h>

#include "ui/tv_view.hpp"

namespace bw {

namespace {

constexpr SDL_FColor kClear{0, 0, 0, 1};
constexpr std::uint8_t kBackdrop = 40;  // the composed screenshot's desktop

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

}  // namespace

GpuRender::GpuRender(SDL_Window* main) : main_(main) {
    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                      SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
                                  false, nullptr);
    if (device_ == nullptr) {
        return;
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, main_)) {
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
        return;
    }
    SDL_SetGPUSwapchainParameters(device_, main_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);
    format_ = SDL_GetGPUSwapchainTextureFormat(device_, main_);
}

GpuRender::~GpuRender() {
    if (device_ != nullptr) {
        SDL_WaitForGPUIdle(device_);
        SDL_ReleaseWindowFromGPUDevice(device_, main_);
        SDL_DestroyGPUDevice(device_);
    }
}

void GpuRender::present(TvView& tv) {
    ImDrawData* data = ImGui::GetDrawData();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    tv.upload(commands);
    SDL_GPUTexture* swapchain = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(commands, main_, &swapchain, nullptr, nullptr);
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
    SDL_PixelFormat pixels = SDL_PIXELFORMAT_UNKNOWN;
    if (format_ == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
        pixels = SDL_PIXELFORMAT_BGRA32;
    } else if (format_ == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
        pixels = SDL_PIXELFORMAT_RGBA32;
    } else {
        return SDL_SetError("screenshot: swapchain format %d", static_cast<int>(format_));
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
    SDL_WaitForGPUIdle(device_);
    for (ImGuiViewportP* viewport : order) {
        ImDrawData* data = viewport->DrawData;
        if (data == nullptr || !data->Valid || data->DisplaySize.x <= 0 ||
            (viewport->Flags & ImGuiViewportFlags_IsMinimized) != 0) {
            continue;
        }
        Surface surface = ReadBack(device_, format_, pixels, data);
        if (!surface) {
            return false;
        }
        shots.push_back({std::move(surface), viewport->Pos, viewport->Size});
    }
    SDL_WaitForGPUIdle(device_);
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
