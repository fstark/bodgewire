#include "ui/board_tags.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "netlist/builder.hpp"
#include "parts/switch.hpp"

namespace bw {

namespace {

// A switch's tag: its kind and legend from the design Circuit's Switch.
std::optional<SwitchTag> SwitchTagOf(const BoardScene& scene, const LoadedCircuit& design,
                                     const LoadedPart& part, bool pressed) {
    const auto component = scene.componentIndex(part.reference);
    if (!component) {
        return std::nullopt;
    }
    const Switch* sw = SwitchOf(design, part.reference);
    if (sw == nullptr) {
        return std::nullopt;
    }
    return SwitchTag{*component, pressed, sw->kind(), std::string(sw->legend())};
}

std::vector<SwitchTag> SwitchTags(const BoardScene& scene, const LoadedCircuit& design,
                                  const Session* session) {
    std::vector<SwitchTag> tags;
    for (const LoadedPart& part : design.parts) {
        if (part.info->kind != PartKind::Switch) {
            continue;
        }
        const bool pressed = session != nullptr && session->pressed(part.reference);
        if (auto tag = SwitchTagOf(scene, design, part, pressed)) {
            tags.push_back(std::move(*tag));
        }
    }
    return tags;
}

}  // namespace

std::vector<LedTag> LedTagsOf(const BoardScene& scene, const Session& session) {
    std::vector<LedTag> tags;
    for (const LedLight& led : session.leds()) {
        if (const auto component = scene.componentIndex(led.reference)) {
            tags.push_back({*component, led.shape, led.brightness});
        }
    }
    return tags;
}

std::vector<SwitchTag> SwitchTagsOf(const BoardScene& scene, const Session& session) {
    if (session.machine() == nullptr) {
        return {};
    }
    return SwitchTags(scene, session.machine()->designCircuit(), &session);
}

std::vector<LedTag> StillLedTags(const BoardScene& scene, const LoadedCircuit& design) {
    std::vector<LedTag> tags;
    for (const LoadedPart& part : design.parts) {
        const auto component = scene.componentIndex(part.reference);
        if (!component) {
            continue;
        }
        for (const Lamp& lamp : part.info->lamps) {
            tags.push_back({*component, lamp.shape, 0.0f});
        }
    }
    return tags;
}

std::vector<SwitchTag> StillSwitchTags(const BoardScene& scene, const LoadedCircuit& design) {
    return SwitchTags(scene, design, nullptr);
}

void ShowRepairs(BoardView& view, const Machine& machine) {
    std::vector<CutMark> cuts;
    for (const Crack& cut : machine.cuts()) {
        cuts.push_back({cut.layer, cut.at});
    }
    view.setCuts(cuts);
    std::vector<CutMark> cracks;
    for (const Crack& crack : machine.cracks()) {
        cracks.push_back({crack.layer, crack.at});
    }
    view.setCracks(cracks);
    std::vector<BodgeWire> wires;
    for (const WireEnds& w : machine.wires()) {
        wires.push_back(w.wire);
    }
    view.setBodgeWires(wires);
    std::vector<BodgeWire> bridges;
    for (const WireEnds& b : machine.bridges()) {
        bridges.push_back(b.wire);
    }
    view.setBridges(bridges);
}

}  // namespace bw
