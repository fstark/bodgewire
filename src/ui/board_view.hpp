#pragma once

#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "session/session.hpp"
#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

enum class Tool : std::uint8_t { Probe, Swap, Mend };

struct BoardClick {
    Point at;                              // nm
    Layer layer;                           // Active Layer at the click
    bool onCopper;                         // a pad, via or Trace of the Active Layer
    std::optional<std::size_t> component;  // componentAt(at)
};

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);       // child region: input, Board, tooltip
    void controls(bool powered);  // tools, Active Layer, Fit; "power off to repair"
    Tool tool() const { return tool_; }
    std::optional<BoardClick> takeClick();       // left click since the last call
    void setProbeTip(std::optional<Point> tip);  // drawn above everything
    void setBlobs(std::span<const SolderBlob> blobs) { blobs_.assign(blobs.begin(), blobs.end()); }

private:
    void input(ImVec2 origin);  // zoom, pan, L, F, P, S, M, left click while the panel is hovered

    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    Tool tool_ = Tool::Probe;
    bool needFit_ = true;  // first draw knows the panel size
    ImVec2 panel_{};
    std::optional<NetId> highlight_;
    std::optional<BoardClick> click_;
    std::optional<Point> tip_;
    std::vector<SolderBlob> blobs_;
};

}  // namespace bw
