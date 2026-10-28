#pragma once

#include <optional>

#include <imgui.h>

#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);  // child region: input, Board, tooltip
    void controls();         // Active Layer (F.Cu | B.Cu) and Fit, one row

private:
    void input(ImVec2 origin);  // zoom, pan, L, F while the panel is hovered

    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    bool needFit_ = true;  // first draw knows the panel size
    ImVec2 panel_{};
    std::optional<NetId> highlight_;
};

}  // namespace bw
