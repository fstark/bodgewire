#include "ui/chip_panel.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "parts/catalogue.hpp"
#include "ui/controls.hpp"
#include "ui/page_fonts.hpp"
#include "ui/touch.hpp"
#include "view/digit_shape.hpp"
#include "view/pin_marks.hpp"

namespace bw {

namespace {

constexpr float kBodyWidth = 34;
constexpr ImU32 kBody = IM_COL32(60, 60, 66, 255);
constexpr ImU32 kNotch = IM_COL32(25, 25, 28, 255);
constexpr ImU32 kPinOne = IM_COL32(220, 220, 220, 255);
constexpr ImVec4 kPowerText{0.5f, 0.48f, 0.44f, 1.0f};    // on paper
constexpr ImVec4 kNumberText{0.45f, 0.43f, 0.40f, 1.0f};  // a pin's number, quieter than names
constexpr ImU32 kDigitBody = IM_COL32(28, 28, 30, 255);
constexpr ImU32 kSegmentDim = IM_COL32(70, 22, 22, 255);
constexpr ImU32 kSegmentLit = IM_COL32(255, 40, 30, 255);
constexpr ImU32 kPad = IM_COL32(200, 200, 190, 255);
constexpr float kDigitLines = 7;     // body height in text lines
constexpr float kTitleScale = 1.3f;  // the page title's, as the manuals'

constexpr float kStub = 20;    // a pin's line between its number and the body (P8j)
constexpr float kStubGap = 4;  // between the line and the number
// One colour for every pin's symbol (P8j), a colour the Board gives no meaning: not the levels'
// red, blue or yellow, the hovered Net's yellow, nor the clips' magenta or blue.
constexpr ImU32 kSymbol = IM_COL32(138, 79, 216, 255);
constexpr ImU32 kPowerLine = IM_COL32(128, 122, 112, 255);
constexpr ImVec4 kAloneText{0.66f, 0.64f, 0.58f, 1.0f};       // a pin that goes nowhere: faint
constexpr ImU32 kGroupTint = IM_COL32(236, 226, 250, 255);    // what a hovered pin works with
constexpr ImU32 kHoveredTint = IM_COL32(220, 202, 246, 255);  // the hovered pin

struct PinRow {
    std::optional<std::size_t> pad;
    std::string number, name, net;  // net "" when none, or when it reaches no other pad unnamed
    bool power = false;
    PinLook look;
    bool alone = false;  // its Net reaches no other pad: it goes nowhere
    ImU32 tint = 0;      // the hovered pin's group (P8j), 0 none
};

// The pads a hovered pin works with (WorksWith) and the hovered one, for the rows' tints.
struct Group {
    std::vector<std::size_t> pads;
    std::optional<std::size_t> hovered;
};

PinRow Row(const BoardScene& scene, std::size_t component, std::size_t number,
           const Group& group = {}) {
    PinRow row;
    row.pad = scene.padIndex(component, std::to_string(number));
    row.number = std::to_string(number);
    if (row.pad) {
        row.name = scene.pinName(component, *row.pad);
        const auto net = scene.padNet(component, *row.pad);
        row.net = net ? std::string(scene.netName(*net)) : "";
        row.power = scene.powerPin(component, *row.pad);
        row.look = LookOf(row.name, scene.pinKinds(component)[*row.pad], scene.chip(component));
        row.alone = scene.alone(component, *row.pad);
        if (group.hovered == row.pad) {
            row.tint = kHoveredTint;
        } else if (std::ranges::find(group.pads, *row.pad) != group.pads.end()) {
            row.tint = kGroupTint;
        }
    }
    if (row.name.empty()) {
        row.name = "-";
    }
    return row;
}

// A pin's line from the body's edge `edge` outwards (`side` -1 left, +1 right) at `y`, its
// symbol on it (P8j): an arrow into the body (in), out of it (out, hollow for open collector),
// both; a bubble at the body (active low); a pulse over it (a clock); dashed when it goes
// nowhere; plain grey for power; nothing for NC.
void DrawPinSymbol(ImDrawList& list, float edge, float y, float side, const PinLook& look,
                   bool alone) {
    if (look.mark == PinMark::NoConnect) {
        return;
    }
    const float outer = edge + side * kStub;
    if (look.mark == PinMark::Power) {
        list.AddLine({edge, y}, {outer, y}, kPowerLine, 2.0f);
        return;
    }
    constexpr float kBubble = 3.6f, kHead = 5.0f;
    const float inner = look.activeLow ? edge + side * kBubble * 2 : edge;
    if (alone) {
        for (float x = 0; x < kStub - (inner - edge) * side; x += 6) {
            const float a = inner + side * x;
            list.AddLine({a, y}, {a + side * std::min(3.0f, std::abs(outer - a)), y}, kSymbol,
                         1.5f);
        }
    } else {
        list.AddLine({inner, y}, {outer, y}, kSymbol, 1.5f);
    }
    // An arrowhead at `tip` pointing `dir` (+1 right, -1 left).
    const auto head = [&](float tip, float dir, bool hollow) {
        const ImVec2 a{tip, y}, b{tip - dir * kHead * 1.2f, y - kHead},
            c{tip - dir * kHead * 1.2f, y + kHead};
        if (hollow) {
            list.AddTriangleFilled(a, b, c, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xff000000);
            list.AddTriangle(a, b, c, kSymbol, 1.5f);
        } else {
            list.AddTriangleFilled(a, b, c, kSymbol);
        }
    };
    const float middle = (inner + outer) / 2;
    switch (look.mark) {
    case PinMark::In:
        head(middle - side * kHead * 0.6f, -side, false);
        break;
    case PinMark::Out:
    case PinMark::OpenCollector:
        head(middle + side * kHead * 0.6f, side, look.mark == PinMark::OpenCollector);
        break;
    case PinMark::Both:
        head(inner + side * 1.0f, -side, false);
        head(outer - side * 1.0f, side, false);
        break;
    default:
        break;
    }
    if (look.activeLow) {
        list.AddCircleFilled({edge + side * kBubble, y}, kBubble, IM_COL32(246, 242, 228, 255));
        list.AddCircle({edge + side * kBubble, y}, kBubble, kSymbol, 0, 1.5f);
    }
    if (look.clock) {  // a pulse over the line
        const float x = middle, top = y - 13, base = y - 7;
        const ImVec2 pulse[] = {{x - 6, base}, {x - 3, base}, {x - 3, top},
                                {x + 3, top},  {x + 3, base}, {x + 6, base}};
        list.AddPolyline(pulse, 6, kSymbol, 0, 1.5f);
    }
}

// After a pin's Selectable: fills `hits` and shows a tooltip: the pin's number, name and Net in
// full (a long row is cut at its cell's edge), then its Datasheet line.
void PinHits(bool clicked, const PinRow& row, std::size_t component, const Markdown* datasheet,
             TargetHits& hits) {
    if (clicked && row.pad) {
        hits.clicked = BoardTarget{{component}, row.pad, std::nullopt, {}};
    }
    // Held down on it, not dragged nor slid off (P8g: the meter measures it meanwhile).
    const ImGuiIO& io = ImGui::GetIO();
    if (row.pad && ImGui::IsItemActive() && ImGui::IsItemHovered() && !PressCancelled() &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
            io.MouseDragThreshold * io.MouseDragThreshold) {
        hits.held = BoardTarget{{component}, row.pad, std::nullopt, {}};
    }
    if (ImGui::IsItemHovered() && row.pad) {
        hits.hovered = BoardTarget{{component}, row.pad, std::nullopt, {}};
        std::string tip = std::format("pin {} \u00b7 {}", row.number, row.name);
        if (!row.net.empty() && row.net != "-") {
            tip += " \u00b7 " + row.net;
        }
        if (const Block* bullet = datasheet ? PinBullet(*datasheet, row.name) : nullptr) {
            tip += "\n" + Displayable(PlainText(bullet->spans));
        }
        PageTooltip(tip);
    }
}

// `more`'s hits where `hits` has none (the first drawn wins).
void Merge(TargetHits& hits, const TargetHits& more) {
    hits.hovered = hits.hovered ? hits.hovered : more.hovered;
    hits.clicked = hits.clicked ? hits.clicked : more.clicked;
    hits.held = hits.held ? hits.held : more.held;
}

// A crosshair centred on `centre`, `radius` its ring's.
void DrawCrosshair(ImDrawList& list, ImVec2 centre, float radius, ImU32 colour) {
    list.AddCircle(centre, radius, colour, 0, 1.5f);
    const float in = radius * 0.45f, out = radius * 1.45f;
    for (const ImVec2 d : {ImVec2{1, 0}, ImVec2{-1, 0}, ImVec2{0, 1}, ImVec2{0, -1}}) {
        list.AddLine({centre.x + d.x * in, centre.y + d.y * in},
                     {centre.x + d.x * out, centre.y + d.y * out}, colour, 1.5f);
    }
}

// A pin's "show" button (P8f): frames the pin on the Board whatever tool is in hand, as a Ctrl-
// or Cmd-click on its row does. A crosshair (2026-11-27: the word "show" took the names' room at
// the larger reading sizes), kControl wide, as high as a pin's row; `width` 0: kControl.
float ShowWidth() { return kControl; }
void ShowButton(const PinRow& row, std::size_t component, TargetHits& hits, float width = 0) {
    const ImVec2 size{width > 0 ? width : ShowWidth(), ControlHeight()};
    if (!row.pad) {
        ImGui::Dummy(size);
        return;
    }
    ImGui::PushID(row.number.c_str());
    if (ImGui::Button("##show", size)) {
        hits.clicked = BoardTarget{{component}, row.pad, std::nullopt, {}};
        hits.frame = true;
    }
    if (ImGui::IsItemHovered()) {
        PageTooltip("Show on the Board");
    }
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    DrawCrosshair(*ImGui::GetWindowDrawList(), {(min.x + max.x) / 2, (min.y + max.y) / 2},
                  std::min(max.x - min.x, max.y - min.y) * 0.22f,
                  ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::PopID();
}

// Where a pin's row is: left or right of a DIP's body, its number next to the body; or in a
// list, its number first.
enum class PinSide : std::uint8_t { Left, Right, List };

// One pin as a selectable row, `width` wide (0: what is left of the line): its number in a slot
// two digits wide, which keeps the names in a column whatever the widths of the font's digits.
void PinCell(const PinRow& row, PinSide side, std::size_t component, const Markdown* datasheet,
             TargetHits& hits, float width = 0) {
    ImGui::PushID(row.number.c_str());
    // A row as high as a control (P8f): the large way to a pin. Level with the "show" button
    // before it on a table's row, whose frame would lower this row's text by its padding.
    ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0;
    const bool clicked = ImGui::Selectable("##pin", false, 0, {width, ControlHeight()});
    ImGui::PopID();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    if (row.tint != 0) {
        list.AddRectFilled(min, max, row.tint, 3.0f);
    }
    const float slot = ImGui::CalcTextSize("00").x;
    const float gap = ImGui::CalcTextSize("  ").x;
    const float number = ImGui::CalcTextSize(row.number.c_str()).x;
    const float y = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) / 2;
    const ImVec4 ink = row.power   ? kPowerText
                       : row.alone ? kAloneText
                                   : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const ImU32 colour = ImGui::GetColorU32(ink);
    const ImU32 numberColour = ImGui::GetColorU32(row.power ? kPowerText : kNumberText);
    // The pin's name in the mono face next to the body, its Net in the link blue further out
    // (P8j: a pin's and a Net's names apart, `CLK  CLK` no longer a repeat).
    PushPageFont(PageFace::Mono);
    ImFont* mono = ImGui::GetFont();
    const float monoSize = ImGui::GetFontSize();
    const float name = ImGui::CalcTextSize(row.name.c_str()).x;
    ImGui::PopFont();
    const float monoY = min.y + (max.y - min.y - monoSize) / 2;
    const float net = ImGui::CalcTextSize(row.net.c_str()).x;
    const ImU32 netColour = row.power ? colour : kPageLink;
    // A row too long for its cell is cut a gap short of the "show" key beside it (its tooltip
    // shows it whole); the number, next to the body, never.
    const float margin = ImGui::GetStyle().ItemSpacing.x;
    list.PushClipRect({side == PinSide::Left ? min.x + margin : min.x, min.y},
                      {side == PinSide::Left ? max.x : max.x - margin, max.y}, true);
    switch (side) {
    case PinSide::Left: {  // "net  name  NN|": both to the right
        list.AddText({max.x - number, y}, numberColour, row.number.c_str());
        const float nameX = max.x - slot - gap - name;
        list.AddText(mono, monoSize, {nameX, monoY}, colour, row.name.c_str());
        list.AddText({nameX - gap - net, y}, netColour, row.net.c_str());
        break;
    }
    case PinSide::Right: {  // "|NN  name  net"
        list.AddText({min.x, y}, numberColour, row.number.c_str());
        const float nameX = min.x + slot + gap;
        list.AddText(mono, monoSize, {nameX, monoY}, colour, row.name.c_str());
        list.AddText({nameX + name + gap, y}, netColour, row.net.c_str());
        break;
    }
    case PinSide::List: {  // "NN —▶ name  net", the numbers to the right of their slot
        list.AddText({min.x + slot - number, y}, numberColour, row.number.c_str());
        const float edge = min.x + slot + gap / 2;
        DrawPinSymbol(list, edge + kStub, (min.y + max.y) / 2, -1, row.look, row.alone);
        const float nameX = edge + kStub + gap / 2;
        list.AddText(mono, monoSize, {nameX, monoY}, colour, row.name.c_str());
        list.AddText({nameX + name + gap, y}, netColour, row.net.c_str());
        break;
    }
    }
    list.PopClipRect();
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

void ChipPanel::remember(const TargetHits& hits, std::size_t component) {
    shown_ = component;
    const auto& hovered = hits.hovered;
    hoveredPad_ =
        hovered && hovered->components.size() == 1 && hovered->components.front() == component
            ? hovered->pad
            : std::nullopt;
}

TargetHits ChipPanel::draw(const BoardScene& scene, std::size_t component, const Manuals& manuals,
                           const LoadedCircuit& design) {
    TargetHits hits;
    const std::string& reference = scene.board().components[component].reference;
    const std::string_view part = scene.partName(component);
    const PartInfo* info = FindPart(part);
    // The title is the chip (P8j): its reference a link (hovered: lit on the Board; clicked:
    // framed and selected), its Part and what it is.
    PushPageFont(PageFace::Bold, kTitleScale);
    {
        TextFlow flow;
        const TextFlow::Result link = flow.text(reference, kPageLink, true);
        const BoardTarget chip{{component}, std::nullopt, std::nullopt, {}};
        if (link.hovered) {
            hits.hovered = chip;
        }
        if (link.clicked) {
            hits.clicked = chip;
        }
        flow.text(std::format(" \u00b7 {} \u00b7 {}", part,
                              info ? info->description : std::string_view{}),
                  ImGui::GetColorU32(ImGuiCol_Text), false);
    }
    ImGui::PopFont();
    ImGui::Separator();
    // Its role on this Machine (P8j): the Service Manual's sentence, with its links.
    if (manuals.service) {
        if (const auto role = ComponentRoleSpans(*manuals.service, reference)) {
            Merge(hits, FlowSpans(*role, &scene, design, LinkScope::Machine));
        }
    }

    // What the pin hovered last frame works with (P8j): its row or a sheet's pin name, known
    // only after this page drew its rows: tinted one frame late.
    Group group;
    if (hoveredPad_ && shown_ == component) {
        group.hovered = hoveredPad_;
        group.pads = WorksWith(scene.pinNames(component), scene.pinKinds(component), *hoveredPad_);
    }
    ImGui::Dummy({0, kBodyWidth / 5 + 4});  // room for the body's notch above its first pin
    const auto sheet = manuals.datasheets.find(part);
    const Markdown* datasheet = sheet != manuals.datasheets.end() ? &sheet->second : nullptr;
    const std::size_t pins = scene.board().components[component].pads.size();
    const bool dip = info && info->package.starts_with("DIP") && pins % 2 == 0;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp;
    if (dip && ImGui::BeginTable("pinout", 5, flags)) {
        ImGui::TableSetupColumn("show left", ImGuiTableColumnFlags_WidthFixed, ShowWidth());
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("body", ImGuiTableColumnFlags_WidthFixed,
                                kBodyWidth + 2 * (kStub + kStubGap));
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("show right", ImGuiTableColumnFlags_WidthFixed, ShowWidth());
        for (std::size_t r = 0; r < pins / 2; ++r) {
            const PinRow left = Row(scene, component, r + 1, group);
            const PinRow right = Row(scene, component, pins - r, group);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ShowButton(left, component, hits);
            ImGui::TableNextColumn();
            PinCell(left, PinSide::Left, component, datasheet, hits);
            ImGui::TableNextColumn();
            const ImVec2 cell = ImGui::GetCursorScreenPos();
            const ImVec2 at{cell.x + kStub + kStubGap, cell.y};  // the body, between the lines
            const float height = ControlHeight() + ImGui::GetStyle().ItemSpacing.y;
            ImDrawList& list = *ImGui::GetWindowDrawList();
            list.AddRectFilled({at.x, at.y - 2}, {at.x + kBodyWidth, at.y + height}, kBody);
            if (r == 0) {
                list.AddCircleFilled({at.x + kBodyWidth / 2, at.y - 2}, kBodyWidth / 5, kNotch);
                list.AddCircleFilled({at.x + 6, at.y + ControlHeight() / 2}, 3, kPinOne);
            }
            const float y = at.y + ControlHeight() / 2;
            if (left.pad) {
                DrawPinSymbol(list, at.x, y, -1, left.look, left.alone);
            }
            if (right.pad) {
                DrawPinSymbol(list, at.x + kBodyWidth, y, 1, right.look, right.alone);
            }
            ImGui::Dummy({kBodyWidth + 2 * (kStub + kStubGap), ControlHeight()});
            ImGui::TableNextColumn();
            PinCell(right, PinSide::Right, component, datasheet, hits);
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
            const PinRow row = Row(scene, component, n, group);
            PinCell(row, PinSide::List, component, datasheet, hits, width);
            ImGui::SameLine();
            ShowButton(row, component, hits);
        }
    }

    if (!datasheet) {
        ImGui::TextDisabled("no datasheet");
        remember(hits, component);
        return hits;
    }
    // Function, then Pins, both open (P8j: folding only hid them).
    for (const auto& [heading, view] :
         {std::pair{"Function", &function_}, std::pair{"Pins", &pins_}}) {
        ImGui::Spacing();
        PushPageFont(PageFace::Bold);
        ImGui::TextUnformatted(heading);
        ImGui::PopFont();
        Merge(hits,
              view->draw(Section(*datasheet, heading), &scene, design, LinkScope::Chip, reference));
    }
    remember(hits, component);
    return hits;
}

}  // namespace bw
