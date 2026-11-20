#include "ui/shelf.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ui/bench.hpp"
#include "ui/brief_window.hpp"
#include "ui/instrument_marks.hpp"
#include "ui/knob_window.hpp"
#include "ui/manual_window.hpp"
#include "ui/tv_window.hpp"
#include "ui/window_order.hpp"
#include "view/machine_slot.hpp"

namespace bw {

namespace {

constexpr auto kReasonTime = std::chrono::seconds(2);
constexpr float kCell = kShelfHeight - 16.0f;  // a square cell's side
constexpr float kCellGap = 4.0f;
constexpr float kGroupGap = 20.0f;
constexpr float kGrayed = 0.35f;
constexpr ImU32 kShelfWood = IM_COL32(48, 38, 30, 255);
constexpr ImU32 kShelfEdge = IM_COL32(92, 74, 58, 255);
// The trays (P6o): recessed, a shade lighter than the wood, a dark line along the top.
constexpr float kTrayPad = 4.0f;
constexpr ImU32 kTray = IM_COL32(62, 50, 40, 255);
constexpr ImU32 kTrayShadow = IM_COL32(28, 22, 17, 255);
constexpr ImU32 kTrayDivider = IM_COL32(36, 29, 23, 255);
constexpr ImU32 kHover = IM_COL32(255, 255, 255, 40);
constexpr ImVec4 kAmber{1.0f, 0.7f, 0.1f, 1.0f};
constexpr ImVec4 kPlain{0.92f, 0.92f, 0.92f, 1.0f};

enum class Tray : std::uint8_t { Controls, Tools, Instruments, Machine };

struct Entry {
    Item item;
    const char* name;
    Tray tray;
    float width;  // in cells; the Machine slot's from its devices
};

// The fixed order, the same on every Machine.
constexpr std::array<Entry, 14> kItems{{
    {Item::Power, "Power", Tray::Controls, 1},
    {Item::RunSwitch, "Pause, real time, or as fast as possible", Tray::Controls, 2},
    {Item::Pencil,
     "Grease pencil: mark the Board; click a mark: dot, check, cross, none; shift- or "
     "right-click: clear",
     Tray::Tools, 1},
    {Item::Puller, "Chip puller: swap a chip", Tray::Tools, 1},
    {Item::Iron, "Soldering iron: mend a crack, resolder a chip pin", Tray::Tools, 1},
    {Item::Cutters, "Cutters: cut a trace or a wire", Tray::Tools, 1},
    {Item::Wire, "Wire: solder a bodge wire between two pins", Tray::Tools, 1},
    {Item::Probe, "Logic probe: touch any copper; HIGH, LOW, PULSE", Tray::Instruments, 2},
    {Item::Analyzer, "Logic Analyzer: click a chip pin; shift-click: its whole bus",
     Tray::Instruments, 1},
    {Item::Scope, "Oscilloscope: click a chip pin (4 channels)", Tray::Instruments, 1},
    {Item::Meter, "Multimeter: volts, or continuity with the power off", Tray::Instruments, 1},
    {Item::Brief, "Puzzle brief", Tray::Machine, 1},
    {Item::Manual, "Manuals binder: User and Service Manuals, datasheets, repairs", Tray::Machine,
     1},
    {Item::Machine, "", Tray::Machine, 1},
}};

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
    case Item::Pencil:
        return Owner::Pencil;
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

BenchObject ObjectOf(Owner instrument) {
    return instrument == Owner::Analyzer ? BenchObject::Analyzer
           : instrument == Owner::Scope  ? BenchObject::Scope
                                         : BenchObject::Meter;
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

// Swap, mend, cut and wire need the power off: with it on, they power-cycle the Machine (P6o).
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

// "TV and keyboard", "Knob box": the slot's devices named.
std::string SlotName(bool tv, bool keyboard, bool knob) {
    std::vector<std::string> names;
    if (tv) {
        names.emplace_back("TV");
    }
    if (keyboard) {
        names.emplace_back("keyboard");
    }
    if (knob) {
        names.emplace_back("knob box");
    }
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i) {
        text += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
    }
    if (!text.empty() && text[0] >= 'a') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

}  // namespace

void PowerOn(Session& session, RunSwitch& run, std::uint64_t seed) {
    if (run == RunSwitch::Pause) {
        run = RunSwitch::RealTime;
        session.setSpeed(Speed::RealTime);
    }
    session.powerOn(seed);
}

void Shelf::refused(std::string reason) {
    reason_ = std::move(reason);
    reasonUntil_ = std::chrono::steady_clock::now() + kReasonTime;
}

void Shelf::machineClick(bool second) {
    struct Member {
        bool* out;
        const char* window;
        BenchObject object;
    };
    std::vector<Member> members;
    if (devices_.tv) {
        members.push_back({&tvOut, kTvWindowName, BenchObject::Tv});
    }
    if (devices_.keyboard) {
        members.push_back({&keyboardOut, keyboardWindow, BenchObject::Keyboard});
    }
    if (devices_.knob) {
        members.push_back({&knobOut, kKnobWindowName, BenchObject::Knob});
    }
    std::vector<const char*> group;
    for (const Member& m : members) {
        group.push_back(m.window);
    }
    std::vector<SlotObject> objects;
    for (const Member& m : members) {
        objects.push_back({*m.out, *m.out && InFrontOf(m.window, group), PoppedOut(m.object)});
    }
    if (second) {  // the double-click's second: all out, then all popped out or all back in
        const bool out = PopAll(objects);
        for (std::size_t i = 0; i < members.size(); ++i) {
            *members[i].out = true;
            if (objects[i].popped != out) {
                TogglePop(members[i].object, members[i].window);
            }
            BringToFront(members[i].window);
        }
        return;
    }
    const SlotClick click = ClickSlot(objects);
    for (const Member& m : members) {
        if (click == SlotClick::PutAway) {
            *m.out = false;
        } else {
            *m.out = true;
            BringToFront(m.window);
        }
    }
}

void Shelf::draw(Session& session, BoardView& board, Focus& focus, RunSwitch& run,
                 std::uint64_t seed) {
    const bool powered = session.powered();
    const bool hasBoard = board.scene() != nullptr;
    if (!found_) {  // a Machine's devices never change: found once, not per frame
        devices_ = Devices{session.tv() != nullptr,
                           !session.keyboards().empty() || !session.matrixKeyboards().empty(),
                           !session.knobs().empty()};
        found_ = true;
    }
    const bool hasDevice = devices_.tv || devices_.keyboard || devices_.knob;
    // The brief only on a Puzzle: not grayed, left out.
    std::vector<Entry> items;
    for (const Entry& entry : kItems) {
        if (entry.item != Item::Brief || hasBrief) {
            items.push_back(entry);
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

    // The layout: the left trays from the left edge, the Machine's tray from the right edge.
    const auto cells = [&](const Entry& entry) {
        return entry.item == Item::Machine ? static_cast<float>(MachineSlotCells(
                                                 devices_.tv, devices_.keyboard, devices_.knob))
                                           : entry.width;
    };
    const auto widthOf = [&](const Entry& entry) {
        const float n = cells(entry);
        return kCell * n + kCellGap * (n - 1);
    };
    const float machineGap = kGroupGap / 2;  // the Machine slot set apart in its tray
    float right = 0;
    for (const Entry& entry : items) {
        if (entry.tray == Tray::Machine) {
            right += widthOf(entry) + (entry.item == Item::Machine ? machineGap : kCellGap);
        }
    }
    right -= kCellGap;
    std::vector<float> xs(items.size());
    float x = origin.x + kGroupGap / 2;
    float rightX = origin.x + width - kGroupGap / 2 - right;
    float leftEnd = x;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const Entry& entry = items[i];
        if (i > 0 && items[i - 1].tray != entry.tray && entry.tray != Tray::Machine) {
            x += kGroupGap - kCellGap;
        }
        if (entry.tray == Tray::Machine) {
            if (entry.item == Item::Machine) {
                rightX += machineGap - kCellGap;
            }
            xs[i] = rightX;
            rightX += widthOf(entry) + kCellGap;
        } else {
            xs[i] = x;
            x += widthOf(entry) + kCellGap;
            leftEnd = x - kCellGap;
        }
    }
    const float top = origin.y + (kShelfHeight - kCell) / 2;
    // The trays, under their items.
    for (std::size_t i = 0; i < items.size();) {
        std::size_t j = i;
        while (j + 1 < items.size() && items[j + 1].tray == items[i].tray) {
            ++j;
        }
        const ImVec2 a{xs[i] - kTrayPad, top - kTrayPad};
        const ImVec2 b{xs[j] + widthOf(items[j]) + kTrayPad, top + kCell + kTrayPad};
        list.AddRectFilled(a, b, kTray, 6.0f);
        list.AddLine({a.x + 4, a.y + 1}, {b.x - 4, a.y + 1}, kTrayShadow, 2.0f);
        i = j + 1;
    }

    for (std::size_t i = 0; i < items.size(); ++i) {
        const Entry& entry = items[i];
        const float w = widthOf(entry);
        const ImVec2 min{xs[i], top}, max{xs[i] + w, top + kCell};
        const std::optional<Owner> owner = OwnerOf(entry.item);
        if (entry.item == Item::Machine) {
            const float divider = min.x - machineGap / 2;
            list.AddLine({divider, top + 4}, {divider, top + kCell - 4}, kTrayDivider, 1.0f);
        }

        // Grayed: not on this Machine (inert).
        std::string why;
        bool inert = false;
        ItemLook look;
        switch (entry.item) {
        case Item::Power:
            look.lit = powered;
            look.limit = session.currentLimited();
            break;
        case Item::RunSwitch:
            look.run = run;
            look.powered = powered;
            break;
        case Item::Probe:
            look.lamps = session.lamps();
            break;
        case Item::Manual:
            look.lit = manualOut;
            break;
        case Item::Machine:
            look.slot = {devices_.tv, devices_.keyboard, devices_.knob,
                         tvOut,       keyboardOut,       knobOut};
            inert = !hasDevice;
            why = "No TV, keyboard or knobs on this Machine";
            break;
        case Item::Brief:
            look.lit = briefOut;
            break;
        default:
            break;
        }
        if (owner) {
            inert = !hasBoard;
            why = "No Board";
            if (IsInstrument(*owner)) {
                look.lit = focus.out(*owner);
            } else {
                look.lit = focus.owner() == *owner;  // held: drawn in use (the probe: its lamps)
            }
        }
        look.lit = look.lit && !inert;  // nothing to show: never drawn on
        look.alpha = inert ? kGrayed : 1.0f;

        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(static_cast<int>(entry.item));
        const bool clicked = ImGui::InvisibleButton("##item", {w, kCell}) && !inert;
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            list.AddRectFilled(min, max, kHover, 6.0f);
        }
        DrawItem(list, entry.item, min, max, look);
        if (owner && !inert && focus.owner() == *owner) {
            list.AddRect({min.x - 2, min.y - 2}, {max.x + 2, max.y + 2}, kFocusGold, 6.0f, 0, 2.0f);
        }
        if (hovered) {
            std::string tip(entry.name);
            if (entry.item == Item::Machine) {
                tip = hasDevice
                          ? SlotName(devices_.tv, devices_.keyboard, devices_.knob) +
                                "\nclick: out, to the front, or away\ndouble-click: " +
                                (MachineSlotCells(devices_.tv, devices_.keyboard, devices_.knob) > 1
                                     ? "their own windows, or back"
                                     : "its own window, or back")
                          : std::string("TV, keyboard, knobs");
            }
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
            if (owner && NeedsPowerOff(*owner) && !inert) {
                tip += "\nWith the power on: power-cycles the Machine";
            }
            if (((owner && IsInstrument(*owner)) || entry.item == Item::Manual) && !inert) {
                tip += "\ndouble-click: its own window, or back";
            }
            if (inert && !why.empty()) {
                tip += "\n" + why;
            }
            ImGui::SetTooltip("%s", tip.c_str());
        }
        ImGui::PopID();

        // The second click of a double-click: the object popped out, or back in (P6o).
        const bool second =
            clicked && ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] >= 2;
        if (clicked) {
            switch (entry.item) {
            case Item::Power:
                if (powered) {
                    session.powerOff();
                } else {
                    PowerOn(session, run, seed);
                }
                break;
            case Item::RunSwitch: {
                const float third = (ImGui::GetIO().MousePos.x - min.x) / w * 3;
                const auto chosen =
                    static_cast<RunSwitch>(std::clamp(static_cast<int>(third), 0, 2));
                if (chosen == RunSwitch::Pause) {
                    if (powered) {  // with the power off `‖` cannot be chosen
                        session.runner().pause();
                        run = chosen;
                    }
                    break;
                }
                session.setSpeed(chosen == RunSwitch::Max ? Speed::Max : Speed::RealTime);
                if (powered) {
                    session.runner().resume();
                }
                run = chosen;
                break;
            }
            case Item::Manual:
                if (second) {
                    manualOut = true;
                    TogglePop(BenchObject::Manual, kManualWindowName);
                } else {
                    ToggleWindow(manualOut, kManualWindowName);
                }
                break;
            case Item::Machine:
                machineClick(second);
                break;
            case Item::Brief:
                ToggleWindow(briefOut, kBriefWindowName);  // glued to the Board: no pop-out
                break;
            default:
                if (IsInstrument(*owner) && second) {
                    focus.key(*owner);  // out with the focus
                    BringToFront(WindowOf(*owner));
                    TogglePop(ObjectOf(*owner), WindowOf(*owner));
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
    }

    // The hint, the first that applies, between the instruments and this Machine's tray.
    std::string hint;
    bool amber = false;
    if (std::chrono::steady_clock::now() < reasonUntil_) {
        hint = reason_;
        amber = true;
    } else if (wireStarted) {
        hint = "Wire: click the second pin";
    } else if (focus.owner() == Owner::Meter && session.meterMode() == MeterMode::Continuity &&
               !powered) {
        hint = session.blackNext() ? "Meter: black lead next" : "Meter: red lead next";
    }
    if (!hint.empty()) {
        const float left = leftEnd + kGroupGap;
        const float end = origin.x + width - kGroupGap - right;
        const ImVec2 size = ImGui::CalcTextSize(hint.c_str());
        list.PushClipRect({left, origin.y}, {end, origin.y + kShelfHeight}, true);
        list.AddText({left, origin.y + (kShelfHeight - size.y) / 2},
                     ImGui::ColorConvertFloat4ToU32(amber ? kAmber : kPlain), hint.c_str());
        list.PopClipRect();
    }
    ImGui::EndChild();
}

}  // namespace bw
