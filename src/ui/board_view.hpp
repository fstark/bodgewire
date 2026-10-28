#pragma once

#include <optional>

#include <imgui.h>

#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

struct BoardClick {
    Point at;       // nm
    Layer layer;    // Active Layer at the click
    bool onCopper;  // a pad, via or Trace of the Active Layer
};

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);                      // child region: input, Board, tooltip
    void controls();                             // Active Layer (F.Cu | B.Cu) and Fit, one row
    std::optional<BoardClick> takeClick();       // left click since the last call
    void setProbeTip(std::optional<Point> tip);  // drawn above everything

private:
    void input(ImVec2 origin);  // zoom, pan, L, F, left click while the panel is hovered

    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    bool needFit_ = true;  // first draw knows the panel size
    ImVec2 panel_{};
    std::optional<NetId> highlight_;
    std::optional<BoardClick> click_;
    std::optional<Point> tip_;
};

}  // namespace bw
