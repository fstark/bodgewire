#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "instruments/logic_analyzer.hpp"
#include "instruments/oscilloscope.hpp"
#include "parts/catalogue.hpp"
#include "session/session.hpp"
#include "view/board_scene.hpp"
#include "view/viewport.hpp"

namespace bw {

enum class Tool : std::uint8_t { Probe, Swap, Mend, Cut, Wire, Resolder, Clip, Hook, Meter };

struct CutMark {
    Layer layer;
    Point at;  // nm
};

// Analyzer Channel colours, readable on the green Board; shared with the Analyzer window.
inline constexpr std::array<ImU32, 8> kChannelColours{
    IM_COL32(255, 230, 0, 255),   IM_COL32(0, 230, 255, 255),   IM_COL32(255, 60, 220, 255),
    IM_COL32(255, 140, 0, 255),   IM_COL32(140, 255, 100, 255), IM_COL32(110, 160, 255, 255),
    IM_COL32(255, 150, 180, 255), IM_COL32(255, 255, 255, 255)};

inline ImU32 ChannelColour(std::size_t channel) { return kChannelColours[channel % 8]; }
// A Channel Group's colour, by its index: groups made one after the other differ.
inline ImU32 GroupColour(std::size_t group) { return kChannelColours[(group + 2) % 8]; }

struct ClipTag {
    Point at;  // pad, nm
    std::size_t channel;
    ImU32 colour;  // the Channel's, or its Channel Group's
};

// Oscilloscope Channel colours: yellow, cyan, magenta, blue.
inline constexpr std::array<ImU32, kScopeChannels> kHookColours{
    IM_COL32(255, 230, 0, 255), IM_COL32(0, 230, 255, 255), IM_COL32(255, 60, 220, 255),
    IM_COL32(110, 160, 255, 255)};

struct HookTag {
    Point at;  // pad, nm
    std::size_t channel;
};

struct LeadTag {
    Point at;  // nm
    bool red;
};

struct LedTag {
    std::size_t component;
    LampShape shape;
    float brightness;  // 0 dark .. 1 fully lit
};

// A BUTTON or KEY on the Board (P5i).
struct SwitchTag {
    std::size_t component;
    bool pressed;
    bool key;  // drawn as a keycap with `legend`; else a round button
    std::string legend;
};

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
    bool shift = false;                    // Shift held at the release
};

using TooltipText = std::function<std::string(const BoardScene&, const Hit&)>;

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);  // child region: input, Board, tooltip
    Tool tool() const { return tool_; }
    // Button or key: a pick of the current tool still counts (reopens a closed window).
    void setTool(Tool tool) { tool_ = tool, picked_ = tool; }
    std::optional<Tool> takePicked() { return std::exchange(picked_, std::nullopt); }
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
    void setClips(std::span<const ClipTag> clips) { clips_.assign(clips.begin(), clips.end()); }
    void setHooks(std::span<const HookTag> hooks) { hooks_.assign(hooks.begin(), hooks.end()); }
    void setLeads(std::span<const LeadTag> leads) { leads_.assign(leads.begin(), leads.end()); }
    void setLeds(std::span<const LedTag> leds) { leds_.assign(leds.begin(), leds.end()); }
    void setSwitches(std::span<const SwitchTag> switches) {
        switches_.assign(switches.begin(), switches.end());
    }
    void setLegendFont(ImFont* font) { legendFont_ = font; }   // keycap legends
    std::optional<std::size_t> held() const { return held_; }  // left button down on a body
    std::optional<std::size_t> takeLatchToggle();              // right-clicked body
    void setCuts(std::span<const CutMark> cuts) { cuts_.assign(cuts.begin(), cuts.end()); }
    void setBodgeWires(std::span<const BodgeWire> wires) {
        bodgeWires_.assign(wires.begin(), wires.end());
    }
    void setWireStart(std::optional<Point> start) { wireStart_ = start; }  // rubber band
    // Index of the nearest wire within reach().copper of `at`; for the Cut tool.
    std::optional<std::size_t> wireAt(Point at) const;
    void setBridges(std::span<const BodgeWire> bridges) {
        bridges_.assign(bridges.begin(), bridges.end());
    }
    std::optional<std::size_t> bridgeAt(Point at) const;  // as wireAt()
    // Shows `reason` next to the cursor for a moment (with the banner's own message).
    void refuse(std::string reason);

private:
    struct Flash {
        BoardTarget target;
        std::chrono::steady_clock::time_point until;
    };

    void keys();                // L, F, P, S, M, K, W, C, O, V, +, -, Esc
    void input(ImVec2 origin);  // hover, zoom, pan, click while the panel is hovered or dragged
    PickReach reach() const;

    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    Tool tool_ = Tool::Probe;
    std::optional<Tool> picked_;
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
    std::vector<ClipTag> clips_;
    std::vector<HookTag> hooks_;
    std::vector<LeadTag> leads_;
    std::vector<LedTag> leds_;
    std::vector<SwitchTag> switches_;
    ImFont* legendFont_ = nullptr;
    std::optional<std::size_t> held_, latchToggle_;
    std::vector<CutMark> cuts_;
    std::vector<BodgeWire> bodgeWires_;
    std::vector<BodgeWire> bridges_;
    std::optional<Point> wireStart_;
    TooltipText tooltip_;
    struct Refusal {
        std::string reason;
        ImVec2 at;  // screen
        std::chrono::steady_clock::time_point until;
    };
    std::optional<Refusal> refusal_;
};

}  // namespace bw
