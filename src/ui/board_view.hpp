#pragma once

#include <array>
#include <chrono>
#include <cstdint>
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
#include "parts/switch.hpp"
#include "session/session.hpp"
#include "ui/board_look.hpp"
#include "ui/view_buttons.hpp"
#include "view/board_scene.hpp"
#include "view/focus.hpp"
#include "view/pencil_marks.hpp"
#include "view/viewport.hpp"

namespace bw {

// The Board panel's background, also the launcher's top lane (P7b).
inline constexpr ImU32 kPanelBackground = IM_COL32(20, 20, 24, 255);

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

// A BUTTON, KEY or SLIDE on the Board (P5i, P5l): drawn as a round push button, a keycap with its
// legend, or a slide switch.
struct SwitchTag {
    std::size_t component;
    bool pressed;  // a slide: at ON
    SwitchKind kind;
    std::string legend;
};

struct TargetHits {  // from a side panel or manual this frame
    std::optional<BoardTarget> hovered, clicked;
    bool frame = false;  // `clicked` asks to be framed, as with Ctrl or Cmd (a pin's "show")
};

struct BoardClick {
    Point at;                              // cursor, nm
    Layer active;                          // Active Layer at the click
    std::optional<Pick> pick;              // nullopt: off everything
    std::optional<std::size_t> component;  // the pick's Component, else componentAt(at)
    bool shift = false;                    // Shift held at the release
    bool fromPage = false;  // a pin link in a manual or the chip panel: the selection stays
    // The Wire tool dragged from this pad (`pick`: where it was released); else a plain click.
    std::optional<Pick> from = {};
    bool finger = false;  // the pointer was a finger's (P8f)
    bool loupe = false;   // released from the loupe: its target was shown
};

using TooltipText = std::function<std::string(const BoardScene&, const Hit&)>;

// The Board panel: ImGui drawing and input over a BoardScene and a Viewport.
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene) : scene_(std::move(scene)) {}

    void draw(ImVec2 size);  // child region: input, Board, tooltip
    // The launcher's Board (P7b): no input (no hover, tooltip, click, pan or zoom; the mouse goes
    // through it), no flip corner, the view framed on `box` (filling the panel, centred) at every
    // resize. nullopt: the bench's Board.
    void setStill(std::optional<Bounds> box) {
        still_ = box;
        needFit_ = true;
    }
    // The objects out on the bench when it opens, px from the panel's sides: the fitted Board
    // centred between them (Viewport::setCovered); later moves of the objects change nothing.
    void setCovered(float left, float right) { viewport_.setCovered(left, right); }
    // The focus owner, set each frame: under the chip puller a click on a switch swaps it.
    void setOwner(Owner owner) { owner_ = owner; }
    Layer activeLayer() const { return active_; }
    void toggleLayer() { active_ = active_ == Layer::Front ? Layer::Back : Layer::Front; }
    void fit() { needFit_ = true; }
    // The first fit framed on `box` instead (--zoom, for screenshots); input as usual after.
    void zoomAtStart(Bounds box) { zoomAtStart_ = box; }
    // The plain look (P6 prototype): copper, pads and outlines only, for specialists; else the
    // parts drawn as the real ones, out of the way where the player works.
    void setPlainLook(bool on) { plainLook_ = on; }
    bool plainLook() const { return plainLook_; }
    // Released without dragging; or, the Wire tool pressed on a pad, released after a drag
    // (`from`: that pad, the Board not panned).
    std::optional<BoardClick> takeClick();
    // A memory chip's window clicked (P6p), whatever the tool: no takeClick() for it.
    std::optional<std::size_t> takeContentsClick();
    std::optional<std::size_t> pointedComponent() const { return hovered_ ? hovered_ : selected_; }
    void select(std::size_t component) { selected_ = component; }
    void deselect() { selected_.reset(); }
    std::optional<std::size_t> selected() const { return selected_; }
    // Highlighted like a Board hover at the next draw only: panels draw after the Board.
    void hoverFromOutside(std::optional<BoardTarget> target) { outside_ = target; }
    void focus(BoardTarget target);  // frame + flash 1 s
    // A manual's or chip panel's hits. A clicked pin, the probe or an instrument owning the focus:
    // a click on that pad for takeClick(), flashed in place, the view unmoved; a double-click's
    // second click focus()es it (the first measured). Cmd-click, or any other target or owner:
    // focus() it (a single part also selected).
    void route(const TargetHits& hits);
    const BoardScene* scene() const { return scene_ ? &*scene_ : nullptr; }
    // The Board outline's top-left corner on screen at the last draw (none without a Board).
    std::optional<ImVec2> corner() const { return corner_; }
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
    std::optional<std::size_t> takeLatchToggle();  // right-clicked body; any click on a slide
    void setCuts(std::span<const CrackMark> cuts) { cuts_.assign(cuts.begin(), cuts.end()); }
    // The Puzzle's cracks: their Traces drawn with square ends, a hairline apart.
    void setCracks(std::span<const CrackMark> cracks) {
        cracks_.assign(cracks.begin(), cracks.end());
    }
    // The grease pencil's marks (P6h), drawn whatever the tool or layer.
    void setMarks(std::span<const PencilMark> marks) { marks_.assign(marks.begin(), marks.end()); }
    // Half a mark's drawn size in nm at the current zoom: a click that near is on the mark.
    std::int64_t markReach() const;
    // The Board point of a right-click (released without dragging) since the last call.
    std::optional<Point> takeRightClick();
    void setBodgeWires(std::span<const PadJoin> wires) {
        bodgeWires_.assign(wires.begin(), wires.end());
    }
    void setWireStart(std::optional<Point> start) { wireStart_ = start; }  // rubber band
    // Index of the nearest wire within reach().copper of `at`; for the Cut tool.
    std::optional<std::size_t> wireAt(Point at) const;
    void setBridges(std::span<const PadJoin> bridges) {
        bridges_.assign(bridges.begin(), bridges.end());
    }
    std::optional<std::size_t> bridgeAt(Point at) const;  // as wireAt()
    // Shows `reason` next to the cursor for a moment (with the banner's own message).
    void refuse(std::string reason);
    // What a finger's first tap with a repair tool would do (P8f), named above Board point
    // `at` until cleared (nullopt): "Cut here: tap again".
    void setArmed(std::optional<Point> at, std::string action) {
        armed_ = at ? std::optional(std::pair{*at, std::move(action)}) : std::nullopt;
    }
    // A finger's reach in nm at the current zoom: a second tap that near the first is on it.
    std::int64_t fingerReach() const;

private:
    struct Flash {
        BoardTarget target;
        std::chrono::steady_clock::time_point until;
    };

    void input(ImVec2 origin);  // hover, zoom, pan, click while the panel is hovered or dragged
    void drawBoard(ImDrawList& list, const Viewport& viewport, ImVec2 origin, ImVec2 panel,
                   bool panelPass);
    void drawLoupe(ImDrawList& list, ImVec2 origin, ImVec2 panel);
    PickReach reach() const;
    // Selected and framed so its pins are a finger's size, or it fills most of the panel (P8f).
    void frameChip(std::size_t component);

    std::optional<BoardScene> scene_;
    std::optional<ImVec2> corner_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    bool plainLook_ = false;
    ViewButtons buttons_;  // Fit, Look, Copper, Hold, Flip (P8f)
    bool copper_ = false;  // Copper on: picks go under chip bodies, as with Option held
    bool hold_ = false;    // Hold on: a click latches a Board button
    Owner owner_ = Owner::Probe;
    bool needFit_ = true;  // first draw knows the panel size
    std::optional<Bounds> still_;
    std::optional<Bounds> zoomAtStart_;
    ImVec2 panel_{};
    std::optional<NetId> highlight_;
    std::optional<BoardTarget> hoverTarget_;  // drawn highlighted: hovered pad or Component
    std::optional<std::size_t> hovered_, selected_;
    std::optional<BoardTarget> outside_;
    std::optional<Flash> flash_;
    bool pressed_ = false;                   // left button went down on the panel
    bool cornerPressed_ = false;             // ... on the flip corner (P6o)
    std::optional<std::size_t> framePress_;  // ... a second time on this chip's body (P8f)
    bool loupe_ = false;     // this press became a finger's hold: the loupe is up (P8f)
    std::string loupeText_;  // the pick's name, under the loupe
    std::optional<std::pair<Point, std::string>> armed_;  // setArmed
    std::optional<std::size_t> windowPressed_;            // ... on a memory chip's window (P6p)
    std::optional<std::size_t> contentsClick_;
    bool dragging_ = false;
    std::optional<Pick> wirePress_;  // the Wire tool pressed on this pad: a drag draws a wire
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
    std::vector<CrackMark> cuts_;
    std::vector<CrackMark> cracks_;
    std::vector<PencilMark> marks_;
    std::optional<Point> rightClick_;
    std::vector<PadJoin> bodgeWires_;
    std::vector<PadJoin> bridges_;
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
