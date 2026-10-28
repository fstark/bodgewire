#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "runner/tv_consumer.hpp"

namespace bw {

// TvPicture → streaming SDL texture, drawn as the largest centred 4:3 rectangle.
class TvView {
public:
    explicit TvView(SDL_Renderer* renderer);
    ~TvView();
    TvView(const TvView&) = delete;
    TvView& operator=(const TvView&) = delete;

    void show(const TvPicture& picture);  // uploads the frame
    void draw(ImVec2 available);

private:
    SDL_Texture* texture_;
    std::vector<std::uint8_t> pixels_;  // RGBA
};

}  // namespace bw
