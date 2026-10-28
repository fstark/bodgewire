#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "session/session.hpp"
#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

enum class Tool : std::uint8_t { Probe, Swap, Mend };

struct BoardTarget {
    std::size_t component;
    std::optional<std::size_t> pad;
};

struct TargetHits {  // from a side panel or manual this frame
    std::optional<BoardTarget> hovered, clicked;
};

struct BoardClick {
    Point at;                              // cursor, nm
    Layer active;                          // Active Layer at the click
    std::optional<Pick> pick;              // nullopt: off everything
    std::optional<std::size_t> component;  // the pick's Component, else componentAt(at)
};

using TooltipText = std::function<std::string(const BoardScene&, const Hit&)>;

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);  // child region: input, Board, tooltip
    Tool tool() const { return tool_; }
    void setTool(Tool tool) { tool_ = tool; }
    Layer activeLayer() const { return active_; }
    void toggleLayer() { active_ = active_ == Layer::Front ? Layer::Back : Layer::Front; }
    void fit() { needFit_ = true; }
    std::optional<BoardClick> takeClick();  // released without dragging
    bool takeEscape();                      // Esc pressed since the last call
    std::optional<std::size_t> chip() const { return hovered_ ? hovered_ : selected_; }
    void select(std::size_t component) { selected_ = component; }
    // Highlighted like a Board hover at the next draw only: panels draw after the Board.
    void hoverFromOutside(std::optional<BoardTarget> target) { outside_ = target; }
    void focus(BoardTarget target);  // frame + flash 1 s
    void route(const TargetHits& hits);
    const BoardScene* scene() const { return scene_ ? &*scene_ : nullptr; }
    void setTooltip(TooltipText text) { tooltip_ = std::move(text); }
    void setProbeTip(std::optional<Point> tip);  // drawn above everything
    void setBlobs(std::span<const SolderBlob> blobs) { blobs_.assign(blobs.begin(), blobs.end()); }

private:
    struct Flash {
        BoardTarget target;
        std::chrono::steady_clock::time_point until;
    };

    void keys();                // L, F, P, S, M, +, -, Esc
    void input(ImVec2 origin);  // hover, zoom, pan, click while the panel is hovered or dragged
    PickReach reach() const;

    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    Tool tool_ = Tool::Probe;
    bool needFit_ = true;  // first draw knows the panel size
    ImVec2 panel_{};
    std::optional<NetId> highlight_;
    std::optional<BoardTarget> hoverTarget_;  // drawn highlighted: hovered pad or Component
    std::optional<std::size_t> hovered_, selected_;
    std::optional<BoardTarget> outside_;
    std::optional<Flash> flash_;
    bool pressed_ = false;  // left button went down on the panel
    bool dragging_ = false;
    bool escape_ = false;
    std::optional<BoardClick> click_;
    std::optional<Point> tip_;
    std::vector<SolderBlob> blobs_;
    TooltipText tooltip_;
};

}  // namespace bw
