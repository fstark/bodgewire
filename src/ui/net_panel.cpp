#include "ui/net_panel.hpp"

#include <format>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/manual_view.hpp"

namespace bw {

namespace {

// One Driver or Load as a link to its pad; fills `hits`, tooltip with Part and role.
void Entry(const NetPin& pin, const BoardScene& scene, const Manuals& manuals, TargetHits& hits) {
    const std::string label = FormatNetPin(pin);
    const auto component = scene.componentIndex(pin.reference);
    const auto pad =
        component ? scene.padIndex(*component, std::to_string(pin.pin + 1)) : std::nullopt;
    if (!pad) {
        ImGui::TextUnformatted(label.c_str());
        return;
    }
    ImGui::PushID(label.c_str());
    if (ImGui::Selectable(Displayable(label).c_str())) {
        hits.clicked = BoardTarget{*component, pad};
    }
    ImGui::PopID();
    if (ImGui::IsItemHovered()) {
        hits.hovered = BoardTarget{*component, pad};
        std::string tip(scene.partName(*component));
        if (manuals.service) {
            if (const auto role = ComponentRole(*manuals.service, pin.reference)) {
                tip += " \u00b7 " + role->name;
            }
        }
        ImGui::SetTooltip("%s", Displayable(tip).c_str());
    }
}

void Entries(const std::vector<NetPin>& pins, const BoardScene& scene, const Manuals& manuals,
             TargetHits& hits) {
    if (pins.empty()) {
        ImGui::TextDisabled("nothing");
    }
    for (const NetPin& pin : pins) {
        Entry(pin, scene, manuals, hits);
    }
}

}  // namespace

NetPanelHits NetPanel::draw(const BoardScene& scene, NetId net, const LoadedCircuit& design,
                            const Manuals& manuals) {
    if (cached_ != net) {
        pins_ = DescribeNet(design, scene.netName(net));
        cached_ = net;
    }
    NetPanelHits hits;
    const std::size_t count = pins_.drivers.size() + pins_.loads.size();
    const std::string header = std::format("{} \u00b7 {} pins", pins_.net, count);
    ImGui::TextUnformatted(header.c_str());
    ImGui::SameLine();
    hits.clear = ImGui::SmallButton("Clear");

    ImGui::TextUnformatted("Driven by");
    ImGui::Indent();
    Entries(pins_.drivers, scene, manuals, hits.targets);
    ImGui::Unindent();

    const bool open = pins_.rail ? ImGui::TreeNode("feeds", "Feeds (%zu pins)", pins_.loads.size())
                                 : ImGui::TreeNodeEx("Feeds", ImGuiTreeNodeFlags_DefaultOpen);
    if (open) {
        Entries(pins_.loads, scene, manuals, hits.targets);
        ImGui::TreePop();
    }
    return hits;
}

}  // namespace bw
