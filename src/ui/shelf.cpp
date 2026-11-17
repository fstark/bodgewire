#include "ui/shelf.hpp"

#include <array>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/brief_window.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/knob_window.hpp"
#include "ui/manual_window.hpp"
#include "ui/shelf_drawings.hpp"
#include "ui/tv_window.hpp"
#include "ui/window_order.hpp"

namespace bw {

namespace {

constexpr auto kReasonTime = std::chrono::seconds(2);
constexpr float kCell = kShelfHeight - 16.0f;  // a square cell's side
constexpr float kCellGap = 4.0f;
constexpr float kGroupGap = 20.0f;
constexpr float kGrayed = 0.35f;
constexpr ImU32 kShelfWood = IM_COL32(48, 38, 30, 255);
constexpr ImU32 kShelfEdge = IM_COL32(92, 74, 58, 255);
constexpr ImU32 kFocusGold = IM_COL32(255, 196, 40, 255);    // what a click on a pin does
constexpr ImU32 kFocusBlocked = IM_COL32(230, 50, 40, 255);  // the focus on a tool that can't work
constexpr ImU32 kHover = IM_COL32(255, 255, 255, 40);
constexpr ImU32 kKeyLetter = IM_COL32(200, 190, 175, 200);
constexpr ImVec4 kAmber{1.0f, 0.7f, 0.1f, 1.0f};
constexpr ImVec4 kPlain{0.92f, 0.92f, 0.92f, 1.0f};

struct Entry {
    Item item;
    const char* name;
    const char* key;  // shown in the cell's corner; nullptr: none
    float width;      // in cells
};

// The fixed order, the same on every Machine; a group ends at each nullptr name.
constexpr std::array<Entry, 18> kItems{{
    {Item::Power, "Power", nullptr, 1},
    {Item::Run, "Run / pause", nullptr, 1},
    {Item::Speed, "Speed: real time or as fast as possible", nullptr, 1},
    {Item::Layer, "Copper layer seen and worked on", "L", 1},
    {Item::Fit, "Fit the Board in view", "F", 1},
    {Item::Probe, "Logic probe: touch any copper; HIGH, LOW, PULSE", "P", 2},
    {Item::Puller, "Chip puller: swap a chip", "S", 1},
    {Item::Iron, "Soldering iron: mend a crack, resolder a chip pin", "M", 1},
    {Item::Cutters, "Cutters: cut a trace or a wire", "K", 1},
    {Item::Wire, "Wire: solder a bodge wire between two pins", "W", 1},
    {Item::Tv, "TV", nullptr, 1},
    {Item::Keyboard, "Keyboard", nullptr, 1},
    {Item::Analyzer, "Logic Analyzer: click a chip pin; shift-click: its whole bus", "C", 1},
    {Item::Scope, "Oscilloscope: click a chip pin (4 channels)", "O", 1},
    {Item::Meter, "Multimeter: volts, or continuity with the power off", "V", 1},
    {Item::Manual, "Manuals binder: User and Service Manuals, datasheets, repairs", "U", 1},
    {Item::Knob, "Knob: drag it, mouse wheel, or the arrow keys", nullptr, 1},
    {Item::Brief, "Puzzle brief", nullptr, 1},
}};

bool GroupEnds(Item item) { return item == Item::Fit || item == Item::Wire; }

std::optional<Owner> OwnerOf(Item item) {
    switch (item) {
    case Item::Probe:
        return Owner::Probe;
    case Item::Puller:
        return Owner::Puller;
    case Item::Iron:
        return Owner::Iron;
    case Item::Cutters:
        return Owner::Cutters;
    case Item::Wire:
        return Owner::Wire;
    case Item::Analyzer:
        return Owner::Analyzer;
    case Item::Scope:
        return Owner::Scope;
    case Item::Meter:
        return Owner::Meter;
    default:
        return std::nullopt;
    }
}

const char* WindowOf(Owner instrument) {
    return instrument == Owner::Analyzer ? kAnalyzerWindowName
           : instrument == Owner::Scope  ? kScopeWindowName
                                         : kMeterWindowName;
}

// A window's shelf drawing clicked (not an instrument's): away, out and in front; out behind
// another, in front; in front, away.
void ToggleWindow(bool& out, const char* window) {
    if (out && window != nullptr && InFront(window)) {
        out = false;
    } else {
        out = true;
        if (window != nullptr) {
            BringToFront(window);
        }
    }
}

// A window's shelf drawing double-clicked: out, in front, back at its default place.
void Home(bool& out, const char* window, BenchObject object) {
    out = true;
    if (window != nullptr) {
        BringToFront(window);
    }
    SendHome(object);
}

BenchObject ObjectOf(Owner instrument) {
    return instrument == Owner::Analyzer ? BenchObject::Analyzer
           : instrument == Owner::Scope  ? BenchObject::Scope
                                         : BenchObject::Meter;
}

// Swap, mend, cut and wire need the power off.
bool NeedsPowerOff(Owner owner) {
    return owner == Owner::Puller || owner == Owner::Iron || owner == Owner::Cutters ||
           owner == Owner::Wire;
}

// What the probe's tip touches: "U1 pin 1 CKB · leg", "bare board"; empty when lifted.
std::string ProbeText(const Session& session, const BoardScene* scene) {
    const auto tip = session.probeTip();
    if (!tip || scene == nullptr) {
        return {};
    }
    const auto picked = scene->pick(tip->at, tip->layer, {});
    std::string text = picked ? scene->hoverText(picked->hit) : std::string("bare board");
    if (const auto side =
            picked ? ContactSide(*scene, picked->hit, tip->layer) : std::string_view();
        !side.empty()) {
        text += " · " + std::string(side);
    }
    return text;
}

}  // namespace

void Shelf::refused(std::string reason) {
    reason_ = std::move(reason);
    reasonUntil_ = std::chrono::steady_clock::now() + kReasonTime;
}

void Shelf::draw(Session& session, BoardView& board, Focus& focus, Speed& speed,
                 std::uint64_t seed) {
    const bool powered = session.powered();
    const bool running = powered && session.runner().status().state != RunState::Paused;
    const bool hasBoard = board.scene() != nullptr;
    if (!devices_) {  // a Machine's devices never change: found once, not per frame
        devices_ = Devices{!session.keyboards().empty() || !session.matrixKeyboards().empty(),
                           !session.knobs().empty()};
    }
    const bool hasTv = session.tv() != nullptr;
    const bool hasKeyboard = devices_->keyboard;
    const bool hasKnob = devices_->knob;
    const ImGuiIO& io = ImGui::GetIO();

    // Keys, as the toolbar's before (the typing modes take key events before ImGui).
    if (!io.WantTextInput) {
        if (hasBoard) {
            constexpr std::array<std::pair<ImGuiKey, Owner>, 6> kTools{
                {{ImGuiKey_P, Owner::Probe},
                 {ImGuiKey_S, Owner::Puller},
                 {ImGuiKey_M, Owner::Iron},
                 {ImGuiKey_R, Owner::Iron},
                 {ImGuiKey_K, Owner::Cutters},
                 {ImGuiKey_W, Owner::Wire}}};
            for (const auto& [key, owner] : kTools) {
                if (ImGui::IsKeyPressed(key, false)) {
                    focus.take(owner);
                }
            }
            constexpr std::array<std::pair<ImGuiKey, Owner>, 3> kInstruments{{
                {ImGuiKey_C, Owner::Analyzer},
                {ImGuiKey_O, Owner::Scope},
                {ImGuiKey_V, Owner::Meter},
            }};
            for (const auto& [key, owner] : kInstruments) {
                if (ImGui::IsKeyPressed(key, false)) {
                    focus.key(owner);
                    BringToFront(WindowOf(owner));
                }
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_U, false)) {
            ToggleWindow(manualOut, kManualWindowName);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::BeginChild("shelf", {0, kShelfHeight}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    list.AddRectFilled(origin, {origin.x + width, origin.y + kShelfHeight}, kShelfWood, 4.0f);
    list.AddLine(origin, {origin.x + width, origin.y}, kShelfEdge, 2.0f);

    float x = origin.x + kGroupGap / 2;
    const float top = origin.y + (kShelfHeight - kCell) / 2;
    for (const Entry& entry : kItems) {
        const float w = kCell * entry.width + kCellGap * (entry.width - 1);
        const ImVec2 min{x, top}, max{x + w, top + kCell};
        const std::optional<Owner> owner = OwnerOf(entry.item);

        // Grayed: not on this Machine (inert), or a tool that needs the power off (takeable).
        std::string why;
        bool inert = false;
        ItemLook look;
        switch (entry.item) {
        case Item::Power:
            look.lit = powered;
            look.limit = session.currentLimited();
            break;
        case Item::Run:
            look.lit = running;
            inert = !powered;
            break;
        case Item::Speed:
            look.maxSpeed = speed == Speed::Max;
            break;
        case Item::Layer:
            look.front = board.activeLayer() == Layer::Front;
            inert = !hasBoard;
            break;
        case Item::Fit:
            inert = !hasBoard;
            break;
        case Item::Probe:
            look.lamps = session.lamps();
            break;
        case Item::Tv:
            look.lit = tvOut;
            inert = !hasTv;
            why = "No TV on this Machine";
            break;
        case Item::Keyboard:
            look.lit = keyboardOut;
            inert = !hasKeyboard;
            why = "No keyboard on this Machine";
            break;
        case Item::Manual:
            look.lit = manualOut;
            break;
        case Item::Knob:
            look.lit = knobOut;
            inert = !hasKnob;
            why = "No knob on this Machine";
            break;
        case Item::Brief:
            look.lit = briefOut;
            inert = !hasBrief;
            why = "No brief: not a Puzzle";
            break;
        default:
            break;
        }
        if (owner) {
            inert = !hasBoard;
            why = "No Board";
            if (IsInstrument(*owner)) {
                look.lit = focus.out(*owner);
            }
        }
        const bool unpowered = owner && hasBoard && NeedsPowerOff(*owner) && powered;
        if (unpowered) {
            why = "Power off first";
        }
        const bool grayed = inert || unpowered;
        look.lit = look.lit && !inert;  // nothing to show: never drawn on
        look.alpha = grayed ? kGrayed : 1.0f;

        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(static_cast<int>(entry.item));
        const bool clicked = ImGui::InvisibleButton("##item", {w, kCell}) && !inert;
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            list.AddRectFilled(min, max, kHover, 6.0f);
        }
        DrawItem(list, entry.item, min, max, look);
        if (entry.item == Item::Keyboard && keyboardTyping && !inert) {
            list.AddRect({min.x - 2, min.y - 2}, {max.x + 2, max.y + 2}, kFocusGold, 6.0f, 0, 2.0f);
        }
        if (owner && !inert && focus.owner() == *owner) {
            // Red while the tool that owns the focus cannot work (the power is on).
            list.AddRect({min.x - 2, min.y - 2}, {max.x + 2, max.y + 2},
                         unpowered ? kFocusBlocked : kFocusGold, 6.0f, 0, 2.0f);
        }
        if (entry.key != nullptr) {
            const float size = ImGui::GetFontSize() * 0.8f;
            list.AddText(ImGui::GetFont(), size, {max.x - size * 0.75f, min.y}, kKeyLetter,
                         entry.key);
        }
        if (hovered) {
            std::string tip =
                entry.key ? std::format("{} ({})", entry.name, entry.key) : std::string(entry.name);
            if (entry.item == Item::Power && look.limit) {
                tip += "\nBench supply in current limit";
            }
            if (entry.item == Item::Probe) {
                if (const std::string touched = ProbeText(session, board.scene());
                    !touched.empty()) {
                    tip += "\n" + touched;
                }
            }
            if (owner && IsInstrument(*owner) && !inert) {
                tip += focus.owner() == *owner ? "\nhas the focus" : look.lit ? "\nout" : "\naway";
            }
            const bool object = entry.item == Item::Tv || entry.item == Item::Keyboard ||
                                entry.item == Item::Manual || entry.item == Item::Knob ||
                                entry.item == Item::Brief || (owner && IsInstrument(*owner));
            if (object && !inert) {
                tip += "\ndouble-click: back in its place";
            }
            if (grayed && !why.empty()) {
                tip += "\n" + why;
            }
            ImGui::SetTooltip("%s", tip.c_str());
        }
        ImGui::PopID();

        // The second click of a double-click: the object back at its default place (P6e).
        const bool home =
            clicked && ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] >= 2;
        if (clicked) {
            switch (entry.item) {
            case Item::Power:
                if (powered) {
                    session.powerOff();
                } else {
                    session.powerOn(seed);
                }
                break;
            case Item::Run:
                if (running) {
                    session.runner().pause();
                } else {
                    session.runner().resume();
                }
                break;
            case Item::Speed:
                speed = speed == Speed::Max ? Speed::RealTime : Speed::Max;
                session.setSpeed(speed);
                break;
            case Item::Layer:
                board.toggleLayer();
                break;
            case Item::Fit:
                board.fit();
                break;
            case Item::Tv:
                home ? Home(tvOut, kTvWindowName, BenchObject::Tv)
                     : ToggleWindow(tvOut, kTvWindowName);
                break;
            case Item::Keyboard:
                home ? Home(keyboardOut, keyboardWindow, BenchObject::Keyboard)
                     : ToggleWindow(keyboardOut, keyboardWindow);
                break;
            case Item::Manual:
                home ? Home(manualOut, kManualWindowName, BenchObject::Manual)
                     : ToggleWindow(manualOut, kManualWindowName);
                break;
            case Item::Knob:
                home ? Home(knobOut, kKnobWindowName, BenchObject::Knob)
                     : ToggleWindow(knobOut, kKnobWindowName);
                break;
            case Item::Brief:
                home ? Home(briefOut, kBriefWindowName, BenchObject::Brief)
                     : ToggleWindow(briefOut, kBriefWindowName);
                break;
            default:
                if (IsInstrument(*owner) && home) {
                    focus.key(*owner);  // out with the focus
                    BringToFront(WindowOf(*owner));
                    SendHome(ObjectOf(*owner));
                } else if (IsInstrument(*owner)) {
                    focus.shelfClick(*owner, InFront(WindowOf(*owner)));
                    if (focus.out(*owner)) {
                        BringToFront(WindowOf(*owner));
                    }
                } else {
                    focus.take(*owner);
                }
                break;
            }
        }
        x += w + (GroupEnds(entry.item) ? kGroupGap : kCellGap);
    }

    // The hint, the first that applies.
    std::string hint;
    bool amber = false;
    if (std::chrono::steady_clock::now() < reasonUntil_) {
        hint = reason_;
        amber = true;
    } else if (!typing.empty()) {
        hint = std::format("Typing on {} · F2 to stop", typing);
    } else if (hasBoard && NeedsPowerOff(focus.owner()) && powered) {
        hint = "Power off first";
        amber = true;
    } else if (wireStarted) {
        hint = "Wire: click the second pin";
    } else if (focus.owner() == Owner::Meter && session.meterMode() == MeterMode::Continuity &&
               !powered) {
        hint = session.blackNext() ? "Meter: black lead next" : "Meter: red lead next";
    }
    if (!hint.empty()) {
        const float left = x + kGroupGap / 2;
        const float right = origin.x + width - 8;
        const ImVec2 size = ImGui::CalcTextSize(hint.c_str());
        list.PushClipRect({left, origin.y}, {right, origin.y + kShelfHeight}, true);
        list.AddText({left, origin.y + (kShelfHeight - size.y) / 2},
                     ImGui::ColorConvertFloat4ToU32(amber ? kAmber : kPlain), hint.c_str());
        list.PopClipRect();
    }
    ImGui::EndChild();
}

}  // namespace bw
