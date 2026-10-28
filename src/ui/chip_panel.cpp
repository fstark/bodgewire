#include "ui/chip_panel.hpp"

#include <format>
#include <string>

#include <imgui.h>

#include "parts/catalogue.hpp"

namespace bw {

namespace {

constexpr float kBodyWidth = 34;
constexpr ImU32 kBody = IM_COL32(60, 60, 66, 255);
constexpr ImU32 kNotch = IM_COL32(25, 25, 28, 255);
constexpr ImU32 kPinOne = IM_COL32(220, 220, 220, 255);
constexpr ImVec4 kPowerText{0.55f, 0.55f, 0.55f, 1.0f};

struct PinRow {
    std::optional<std::size_t> pad;
    std::string number, name, net;
    bool power = false;
};

PinRow Row(const BoardScene& scene, std::size_t component, std::size_t number) {
    PinRow row{scene.padIndex(component, std::to_string(number)), std::to_string(number), "", "",
               false};
    if (row.pad) {
        row.name = scene.pinName(component, *row.pad);
        const auto net = scene.padNet(component, *row.pad);
        row.net = net ? std::string(scene.netName(*net)) : "-";
        row.power = scene.powerPin(component, *row.pad);
    }
    if (row.name.empty()) {
        row.name = "-";
    }
    return row;
}

// One pin as a selectable row; fills `hits` and shows the Datasheet line as a tooltip.
void PinCell(const PinRow& row, std::string_view label, bool alignRight, std::size_t component,
             const Markdown* datasheet, TargetHits& hits) {
    if (row.power) {
        ImGui::PushStyleColor(ImGuiCol_Text, kPowerText);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {alignRight ? 1.0f : 0.0f, 0.5f});
    ImGui::PushID(row.number.c_str());
    const std::string text(label);
    if (ImGui::Selectable(text.c_str()) && row.pad) {
        hits.clicked = BoardTarget{component, row.pad};
    }
    ImGui::PopID();
    ImGui::PopStyleVar();
    if (row.power) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered() && row.pad) {
        hits.hovered = BoardTarget{component, row.pad};
        if (datasheet) {
            if (const Block* bullet = PinBullet(*datasheet, row.name)) {
                const std::string line = Displayable(PlainText(bullet->spans));
                ImGui::SetTooltip("%s", line.c_str());
            }
        }
    }
}

}  // namespace

TargetHits ChipPanel::draw(const BoardScene& scene, std::size_t component, const Manuals& manuals,
                           const LoadedCircuit& design) {
    TargetHits hits;
    const std::string& reference = scene.board().components[component].reference;
    const std::string_view part = scene.partName(component);
    const PartInfo* info = FindPart(part);
    const std::string header = std::format("{} \u00b7 {} \u00b7 {}", reference, part,
                                           info ? info->description : std::string_view{});
    ImGui::TextWrapped("%s", header.c_str());
    if (manuals.service) {
        if (const auto role = ComponentRole(*manuals.service, reference)) {
            const std::string text =
                role->detail.empty() ? role->name : role->name + ": " + role->detail;
            ImGui::TextWrapped("%s", Displayable(text).c_str());
        }
    }
    if (ImGui::SmallButton("Zoom to")) {
        hits.clicked = BoardTarget{component, std::nullopt};
    }

    const auto sheet = manuals.datasheets.find(part);
    const Markdown* datasheet = sheet != manuals.datasheets.end() ? &sheet->second : nullptr;
    const std::size_t pins = scene.board().components[component].pads.size();
    const bool dip = info && info->package.starts_with("DIP") && pins % 2 == 0;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp;
    if (dip && ImGui::BeginTable("pinout", 3, flags)) {
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("body", ImGuiTableColumnFlags_WidthFixed, kBodyWidth);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        for (std::size_t r = 0; r < pins / 2; ++r) {
            const PinRow left = Row(scene, component, r + 1);
            const PinRow right = Row(scene, component, pins - r);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            PinCell(left, std::format("{}  {}  {:>2}", left.net, left.name, left.number), true,
                    component, datasheet, hits);
            ImGui::TableNextColumn();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float height = ImGui::GetTextLineHeightWithSpacing();
            ImDrawList& list = *ImGui::GetWindowDrawList();
            list.AddRectFilled({at.x, at.y - 2}, {at.x + kBodyWidth, at.y + height}, kBody);
            if (r == 0) {
                list.AddCircleFilled({at.x + kBodyWidth / 2, at.y - 2}, kBodyWidth / 5, kNotch);
                list.AddCircleFilled({at.x + 6, at.y + height / 2}, 3, kPinOne);
            }
            ImGui::Dummy({kBodyWidth, ImGui::GetTextLineHeight()});
            ImGui::TableNextColumn();
            PinCell(right, std::format("{:<2}  {}  {}", right.number, right.name, right.net), false,
                    component, datasheet, hits);
        }
        ImGui::EndTable();
    } else if (!dip) {
        for (std::size_t n = 1; n <= pins; ++n) {
            const PinRow row = Row(scene, component, n);
            PinCell(row, std::format("{:>2}  {}  {}", row.number, row.name, row.net), false,
                    component, datasheet, hits);
        }
    }

    if (!datasheet) {
        ImGui::TextDisabled("no datasheet");
        return hits;
    }
    if (ImGui::TreeNodeEx("Function", ImGuiTreeNodeFlags_DefaultOpen)) {
        const TargetHits more = function_.draw(Section(*datasheet, "Function"), &scene, design);
        hits.hovered = hits.hovered ? hits.hovered : more.hovered;
        hits.clicked = hits.clicked ? hits.clicked : more.clicked;
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Pins")) {
        pins_.draw(Section(*datasheet, "Pins"), &scene, design);
        ImGui::TreePop();
    }
    return hits;
}

}  // namespace bw
