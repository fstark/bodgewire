#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <imgui.h>

#include "runner/tv_consumer.hpp"
#include "view/tv_controls.hpp"

namespace bw {

// TvPicture → the RGBA pixels the TV window draws (P6d), through the monitor's tone (P6f).
// GpuRender owns the texture they go to and uploads them in the frame, when they changed (P6j).
class TvView {
public:
    explicit TvView(ImTextureID texture);  // GpuRender::tvTexture()

    void show(const TvPicture& picture);  // keeps the frame, refills the pixels through the tone
    void clear();               // a blank frame, no lines: another Machine's picture to come (P7b)
    void setTone(TvTone tone);  // a change refills the kept frame
    int lines() const { return lines_; }  // the last locked frame's line count, 0 before one
    ImTextureID texture() const { return texture_; }

    // kTvColumns × kTvRows RGBA, rows top-down; changed() until GpuRender has uploaded them.
    std::span<const std::uint8_t> pixels() const { return pixels_; }
    bool changed() const { return changed_; }
    void uploaded() { changed_ = false; }

private:
    void fill();

    ImTextureID texture_;
    std::vector<float> cells_;          // the shown frame
    std::vector<std::uint8_t> pixels_;  // RGBA
    TvTone tone_;
    int lines_ = 0;
    bool changed_ = false;
};

}  // namespace bw
