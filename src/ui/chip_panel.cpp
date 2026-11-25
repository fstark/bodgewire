#include "ui/chip_panel.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "parts/catalogue.hpp"
#include "ui/controls.hpp"
#include "ui/touch.hpp"
#include "view/digit_shape.hpp"

namespace bw {

namespace {

constexpr float kBodyWidth = 34;
constexpr ImU32 kBody = IM_COL32(60, 60, 66, 255);
constexpr ImU32 kNotch = IM_COL32(25, 25, 28, 255);
constexpr ImU32 kPinOne = IM_COL32(220, 220, 220, 255);
constexpr ImVec4 kPowerText{0.5f, 0.48f, 0.44f, 1.0f};  // on paper
constexpr ImU32 kDigitBody = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kSegmentDim = IM_COL32(70, 22, 22, 255);
constexpr ImU32 kSegmentLit = IM_COL32(255, 40, 30, 255);
constexpr ImU32 kPad = IM_COL32(200, 200, 190, 255);
constexpr float kDigitLines = 7;  // body height in text lines

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

// After a pin's Selectable: fills `hits` and shows the Datasheet line as a tooltip.
void PinHits(bool clicked, const PinRow& row, std::size_t component, const Markdown* datasheet,
             TargetHits& hits) {
    if (clicked && row.pad) {
        hits.clicked = BoardTarget{{component}, row.pad, std::nullopt};
    }
    // Held down on it, not dragged nor slid off (P8g: the meter measures it meanwhile).
    const ImGuiIO& io = ImGui::GetIO();
    if (row.pad && ImGui::IsItemActive() && ImGui::IsItemHovered() && !PressCancelled() &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
            io.MouseDragThreshold * io.MouseDragThreshold) {
        hits.held = BoardTarget{{component}, row.pad, std::nullopt};
    }
    if (ImGui::IsItemHovered() && row.pad) {
        hits.hovered = BoardTarget{{component}, row.pad, std::nullopt};
        if (datasheet) {
            if (const Block* bullet = PinBullet(*datasheet, row.name)) {
                PageTooltip(Displayable(PlainText(bullet->spans)));
            }
        }
    }
}

// A pin's "show" button (P8f): frames the pin on the Board whatever tool is in hand, as a Ctrl-
// or Cmd-click on its row does. `width` 0: the label's.
constexpr const char* kShow = "show";
float ShowWidth() { return ImGui::CalcTextSize(kShow).x + 2 * ImGui::GetStyle().FramePadding.x; }
void ShowButton(const PinRow& row, std::size_t component, TargetHits& hits, float width = 0) {
    if (!row.pad) {
        ImGui::Dummy({width > 0 ? width : ShowWidth(), kControl});
        return;
    }
    ImGui::PushID(row.number.c_str());
    if (ImGui::Button(kShow, {width, 0})) {
        hits.clicked = BoardTarget{{component}, row.pad, std::nullopt};
        hits.frame = true;
    }
    ImGui::PopID();
}

// One pin as a selectable row, `width` wide (0: what is left of the line).
void PinCell(const PinRow& row, std::string_view label, bool alignRight, std::size_t component,
             const Markdown* datasheet, TargetHits& hits, float width = 0) {
    if (row.power) {
        ImGui::PushStyleColor(ImGuiCol_Text, kPowerText);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {alignRight ? 1.0f : 0.0f, 0.5f});
    ImGui::PushID(row.number.c_str());
    const std::string text(label);
    // A row as high as a control (P8f): the large way to a pin. Level with the "show" button
    // before it on a table's row, whose frame would lower this row's text by its padding.
    ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0;
    const bool clicked = ImGui::Selectable(text.c_str(), false, 0, {width, kControl});
    ImGui::PopID();
    ImGui::PopStyleVar();
    if (row.power) {
        ImGui::PopStyleColor();
    }
    PinHits(clicked, row, component, datasheet, hits);
}

// One pin as a selectable block of centred lines, `width` wide; true while hovered.
bool PinBlock(const PinRow& row, std::span<const std::string> lines, float width,
              std::size_t component, const Markdown* datasheet, TargetHits& hits) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float line = ImGui::GetTextLineHeight();
    ImGui::PushID(row.number.c_str());
    const bool clicked =
        ImGui::Selectable("##pin", false, 0, {width, line * static_cast<float>(lines.size())});
    ImGui::PopID();
    const bool hovered = ImGui::IsItemHovered();
    PinHits(clicked, row, component, datasheet, hits);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImU32 colour =
        ImGui::GetColorU32(row.power ? kPowerText : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    float y = at.y;
    for (const std::string& text : lines) {
        const float w = ImGui::CalcTextSize(text.c_str()).x;
        list.AddText({at.x + (width - w) / 2, y}, colour, text.c_str());
        y += line;
    }
    return hovered;
}

// Packages with one row of pins along the top and one along the bottom (7-segment digits):
// pin 1 bottom left, counting left to right along the bottom, then right to left along the top.
bool TopBottomRows(const PartInfo* info) { return info && info->package.starts_with("DIGIT"); }

// Pins above and below the digit body, the digit in between; the hovered pin's segment lit
// (an anode pin lights them all).
void DigitPinout(const BoardScene& scene, std::size_t component, const PartInfo& info,
                 const Markdown* datasheet, TargetHits& hits) {
    const std::size_t pins = scene.board().components[component].pads.size();
    const std::size_t columns = pins / 2;
    const float width = ImGui::GetContentRegionAvail().x;
    const float cell = width / static_cast<float>(columns);
    const float line = ImGui::GetTextLineHeight();
    const float x0 = ImGui::GetCursorPosX();
    std::optional<std::string> hoveredPin;

    // Each pin's "show" at its outer end: above the top row, below the bottom one.
    const auto shows = [&](bool top) {
        for (std::size_t c = 0; c < columns; ++c) {
            if (c > 0) {
                ImGui::SameLine(x0 + cell * static_cast<float>(c), 0);
            }
            ShowButton(Row(scene, component, top ? pins - c : c + 1), component, hits, cell - 4);
        }
    };
    shows(true);
    // Top row, left to right: pins `pins` .. columns + 1; number next to the body.
    for (std::size_t c = 0; c < columns; ++c) {
        const PinRow row = Row(scene, component, pins - c);
        if (c > 0) {
            ImGui::SameLine(x0 + cell * static_cast<float>(c), 0);
        }
        const std::array<std::string, 3> lines{row.net, row.name, row.number};
        if (PinBlock(row, lines, cell, component, datasheet, hits)) {
            hoveredPin = row.name;
        }
    }
    const ImVec2 body = ImGui::GetCursorScreenPos();
    const float height = line * kDigitLines;
    ImGui::Dummy({width, height});
    // Bottom row, left to right: pins 1 .. columns; number next to the body.
    for (std::size_t c = 0; c < columns; ++c) {
        const PinRow row = Row(scene, component, c + 1);
        if (c > 0) {
            ImGui::SameLine(x0 + cell * static_cast<float>(c), 0);
        }
        const std::array<std::string, 3> lines{row.number, row.name, row.net};
        if (PinBlock(row, lines, cell, component, datasheet, hits)) {
            hoveredPin = row.name;
        }
    }
    shows(false);

    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(body, {body.x + width, body.y + height}, kDigitBody);
    const float pad = line * 0.3f;
    for (std::size_t c = 0; c < columns; ++c) {
        const float x = body.x + cell * (static_cast<float>(c) + 0.5f);
        list.AddCircleFilled({x, body.y + pad * 1.6f}, pad, kPad);
        if (c == 0) {
            list.AddRectFilled({x - pad, body.y + height - pad * 2.6f},
                               {x + pad, body.y + height - pad * 0.6f}, kPad);
        } else {
            list.AddCircleFilled({x, body.y + height - pad * 1.6f}, pad, kPad);
        }
    }

    // The digit in screen pixels scaled to nm (y grows downwards on both).
    constexpr double kScale = 1000;
    const double length = height * 1.1 * kScale;  // the glyph is ~0.6 of its frame
    const DigitFrame frame{(body.x + width / 2) * kScale,
                           (body.y + height / 2) * kScale,
                           0,
                           -1,
                           1,
                           0,
                           length,
                           length * 0.66};
    std::vector<ImVec2> points;
    for (const Lamp& lamp : info.lamps) {
        const bool lit =
            hoveredPin && (std::ranges::find(lamp.cathodes, *hoveredPin) != lamp.cathodes.end() ||
                           std::ranges::find(lamp.anodes, *hoveredPin) != lamp.anodes.end());
        points.clear();
        for (const Point p : LampOutline(frame, lamp.shape)) {
            points.push_back({static_cast<float>(static_cast<double>(p.x) / kScale),
                              static_cast<float>(static_cast<double>(p.y) / kScale)});
        }
        list.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()),
                                 lit ? kSegmentLit : kSegmentDim);
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
    if (ImGui::Button("Zoom to")) {
        hits.clicked = BoardTarget{{component}, std::nullopt, std::nullopt};
    }

    const auto sheet = manuals.datasheets.find(part);
    const Markdown* datasheet = sheet != manuals.datasheets.end() ? &sheet->second : nullptr;
    const std::size_t pins = scene.board().components[component].pads.size();
    const bool dip = info && info->package.starts_with("DIP") && pins % 2 == 0;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp;
    if (dip && ImGui::BeginTable("pinout", 5, flags)) {
        ImGui::TableSetupColumn("show left", ImGuiTableColumnFlags_WidthFixed, ShowWidth());
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("body", ImGuiTableColumnFlags_WidthFixed, kBodyWidth);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("show right", ImGuiTableColumnFlags_WidthFixed, ShowWidth());
        for (std::size_t r = 0; r < pins / 2; ++r) {
            const PinRow left = Row(scene, component, r + 1);
            const PinRow right = Row(scene, component, pins - r);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ShowButton(left, component, hits);
            ImGui::TableNextColumn();
            PinCell(left, std::format("{}  {}  {:>2}", left.net, left.name, left.number), true,
                    component, datasheet, hits);
            ImGui::TableNextColumn();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float height = kControl + ImGui::GetStyle().ItemSpacing.y;
            ImDrawList& list = *ImGui::GetWindowDrawList();
            list.AddRectFilled({at.x, at.y - 2}, {at.x + kBodyWidth, at.y + height}, kBody);
            if (r == 0) {
                list.AddCircleFilled({at.x + kBodyWidth / 2, at.y - 2}, kBodyWidth / 5, kNotch);
                list.AddCircleFilled({at.x + 6, at.y + height / 2}, 3, kPinOne);
            }
            ImGui::Dummy({kBodyWidth, kControl});
            ImGui::TableNextColumn();
            PinCell(right, std::format("{:<2}  {}  {}", right.number, right.name, right.net), false,
                    component, datasheet, hits);
            ImGui::TableNextColumn();
            ShowButton(right, component, hits);
        }
        ImGui::EndTable();
    } else if (TopBottomRows(info) && pins % 2 == 0 && !info->lamps.empty()) {
        DigitPinout(scene, component, *info, datasheet, hits);
    } else if (!dip) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float width = ImGui::GetContentRegionAvail().x - ShowWidth() - spacing;
        for (std::size_t n = 1; n <= pins; ++n) {
            const PinRow row = Row(scene, component, n);
            PinCell(row, std::format("{:>2}  {}  {}", row.number, row.name, row.net), false,
                    component, datasheet, hits, width);
            ImGui::SameLine();
            ShowButton(row, component, hits);
        }
    }

    if (!datasheet) {
        ImGui::TextDisabled("no datasheet");
        return hits;
    }
    if (ImGui::TreeNodeEx("Function",
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding)) {
        const TargetHits more =
            function_.draw(Section(*datasheet, "Function"), &scene, design, LinkScope::Parts);
        hits.hovered = hits.hovered ? hits.hovered : more.hovered;
        hits.clicked = hits.clicked ? hits.clicked : more.clicked;
        hits.held = hits.held ? hits.held : more.held;
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Pins", ImGuiTreeNodeFlags_FramePadding)) {
        pins_.draw(Section(*datasheet, "Pins"), &scene, design, LinkScope::Parts);
        ImGui::TreePop();
    }
    return hits;
}

}  // namespace bw
