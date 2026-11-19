#include "ui/manual_window.hpp"

#include <array>
#include <numbers>
#include <string_view>

#include <imgui.h>

#include "ui/bench.hpp"
#include "view/bench_places.hpp"

namespace bw {

namespace {

constexpr float kAspect = 500.0f / 600.0f;  // FirstSize(Manual): height / width
constexpr float kMinWidth = 500;            // a BW-2001 pinout row needs about 126 px a side
constexpr float kSpine = 34;                // the cover left of the page, the rings' strip
constexpr float kTabStrip = 86;             // right of the page: the tabs and the cover's edge
constexpr float kCoverEdge = 12;            // the cover above, below and right of everything
constexpr float kTabTop = 30;
constexpr float kTabHeight = 44;
constexpr float kTabStep = 50;
constexpr float kTabSetBack = 6;
constexpr float kTabLabelScale = 0.85f;
constexpr float kTitleScale = 1.3f;
constexpr float kPi = std::numbers::pi_v<float>;

constexpr ImU32 kCover = IM_COL32(36, 60, 115, 255);
constexpr ImU32 kCoverEdgeLight = IM_COL32(70, 100, 160, 255);
constexpr ImU32 kPaper = IM_COL32(246, 242, 228, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 60);
constexpr ImU32 kHole = IM_COL32(200, 195, 180, 255);
constexpr ImU32 kRing = IM_COL32(190, 190, 198, 255);
constexpr ImU32 kRingDark = IM_COL32(110, 110, 120, 255);
constexpr ImU32 kInk = IM_COL32(30, 30, 35, 255);
constexpr ImU32 kTabRule = IM_COL32(0, 0, 0, 50);

struct Tab {
    Divider divider;
    const char* label;
    ImU32 colour;
};
constexpr std::array<Tab, 4> kTabs{{
    {Divider::User, "User\nManual", IM_COL32(240, 225, 140, 255)},
    {Divider::Service, "Service\nManual", IM_COL32(170, 215, 160, 255)},
    {Divider::Datasheets, "Datasheets", IM_COL32(165, 195, 235, 255)},
    {Divider::Repairs, "Repairs", IM_COL32(235, 175, 190, 255)},
}};

const char* PageId(Divider divider) {
    switch (divider) {
    case Divider::User:
        return "page user";
    case Divider::Service:
        return "page service";
    case Divider::Datasheets:
        return "page datasheets";
    case Divider::Repairs:
        return "page repairs";
    }
    return "page";
}

ImU32 Scaled(ImU32 colour, float brightness, float alpha = 1) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(colour);
    return ImGui::GetColorU32({c.x * brightness, c.y * brightness, c.z * brightness, c.w * alpha});
}

// The page's colours, pushed around its child window (kPaperColours of them).
void PushPaper() {
    constexpr ImVec4 kTan{0.88f, 0.83f, 0.71f, 1}, kTanHover{0.84f, 0.78f, 0.63f, 1},
        kTanActive{0.80f, 0.73f, 0.57f, 1};
    ImGui::PushStyleColor(ImGuiCol_Text, kInk);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(120, 115, 105, 255));
    ImGui::PushStyleColor(ImGuiCol_Header, kTan);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kTanHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kTanActive);
    ImGui::PushStyleColor(ImGuiCol_Button, kTan);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kTanHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kTanActive);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(190, 180, 160, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(170, 160, 140, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(150, 140, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(200, 190, 170, 255));
}
constexpr int kPaperColours = 13;

void Title(const char* text) {
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

// The cover, the page with its punched holes, and the rings through them.
void DrawBinder(ImDrawList& list, ImVec2 at, float w, float h) {
    list.AddRect(at, {at.x + w, at.y + h}, kCoverEdgeLight, 6.0f, 0, 1.5f);
    const ImVec2 page{at.x + kSpine, at.y + kCoverEdge};
    const ImVec2 pageMax{at.x + w - kTabStrip, at.y + h - kCoverEdge};
    list.AddRectFilled({page.x + 2, page.y + 2}, {pageMax.x + 2, pageMax.y + 2}, kShadow, 2.0f);
    list.AddRectFilled(page, pageMax, kPaper, 2.0f);
    for (const float share : {0.2f, 0.5f, 0.8f}) {
        const float y = at.y + h * share;
        const ImVec2 hole{page.x + 12, y};
        list.AddCircleFilled(hole, 5, kHole);
        // An open ring from the spine to the hole: the upper half of an ellipse, seen from above.
        const ImVec2 centre{(at.x + 22 + hole.x) / 2, y};
        const float rx = (hole.x - (at.x + 22)) / 2;
        list.PathEllipticalArcTo(centre, {rx, 10}, 0, kPi, 2 * kPi);
        list.PathStroke(kRing, 0, 3.0f);
        list.PathEllipticalArcTo(centre, {rx - 1.5f, 8.5f}, 0, kPi, 2 * kPi);
        list.PathStroke(kRingDark, 0, 1.0f);
    }
}

}  // namespace

void ManualWindows::draw(bool& open, const Manuals& manuals, BoardView& board,
                         const LoadedCircuit& design, std::span<const std::string> log) {
    binder_.frame(open, board.selected());
    if (!open) {
        return;
    }
    if (!indexed_) {
        index_ = DatasheetIndex(design);
        indexed_ = true;
    }
    PlaceNext(BenchObject::Manual, {600, 600 * kAspect * stretch_});
    KeepAspectStretched(kAspect, kMinWidth, kManualWindowName, stretch_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kCover);
    const bool visible =
        ImGui::Begin(kManualWindowName, &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!visible) {
        ImGui::End();
        return;
    }
    const ImVec2 at = ImGui::GetWindowPos();
    const float w = ImGui::GetWindowWidth();
    const float h = ImGui::GetWindowHeight();
    DrawBinder(*ImGui::GetWindowDrawList(), at, w, h);
    const BoardScene* scene = board.scene();
    if (binder_.divider() == Divider::Repairs && !scene) {
        binder_.turn(Divider::User);  // no Board, no repairs (--manual repairs on a netlist)
    }
    tabs(scene != nullptr);
    const Divider divider = binder_.divider();
    const ImVec2 page{at.x + kSpine + 24, at.y + kCoverEdge + 10};
    const ImVec2 size{w - kSpine - kTabStrip - 32, h - 2 * kCoverEdge - 20};
    ImGui::SetCursorScreenPos(page);
    PushPaper();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::BeginChild(PageId(divider), size, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground)) {
        switch (divider) {
        case Divider::User:
            if (manuals.user) {
                board.route(user_.draw(manuals.user->blocks, scene, design, LinkScope::Machine));
            } else {
                ImGui::TextDisabled("no user manual");
            }
            break;
        case Divider::Service:
            if (manuals.service) {
                board.route(
                    service_.draw(manuals.service->blocks, scene, design, LinkScope::Machine));
            } else {
                ImGui::TextDisabled("no service manual");
            }
            break;
        case Divider::Datasheets:
            datasheets(manuals, board, design);
            break;
        case Divider::Repairs:
            Title("Repairs");
            if (log.empty()) {
                ImGui::TextDisabled("none yet");
            }
            for (const std::string& entry : log) {
                ImGui::TextUnformatted(entry.c_str());
            }
            break;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(kPaperColours);
    if (CaseClose()) {
        open = false;
    }
    ImGui::End();
}

void ManualWindows::tabs(bool board) {
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 at = ImGui::GetWindowPos();
    const float w = ImGui::GetWindowWidth();
    const float pageRight = at.x + w - kTabStrip;
    const float right = at.x + w - kCoverEdge - 2;
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTabLabelScale);
    for (std::size_t i = 0; i < kTabs.size(); ++i) {
        const Tab& tab = kTabs[i];
        const bool open = binder_.divider() == tab.divider;
        const bool enabled = tab.divider != Divider::Repairs || board;
        const float top = at.y + kTabTop + kTabStep * static_cast<float>(i);
        const ImVec2 min{open ? pageRight - 2 : pageRight + kTabSetBack, top};
        const ImVec2 max{right, top + kTabHeight};
        ImGui::SetCursorScreenPos({pageRight, top});
        ImGui::PushID(static_cast<int>(i));
        const bool clicked = ImGui::InvisibleButton("##tab", {right - pageRight, kTabHeight});
        ImGui::PopID();
        const bool pointed = ImGui::IsItemHovered();
        const bool hovered = enabled && pointed;
        if (clicked && enabled) {
            binder_.turn(tab.divider);
        }
        const float brightness = open ? 1.0f : hovered ? 0.96f : 0.88f;
        list.AddRectFilled(min, max, Scaled(tab.colour, brightness, enabled ? 1.0f : 0.4f), 6.0f,
                           ImDrawFlags_RoundCornersRight);
        list.AddRect(min, max, kTabRule, 6.0f, ImDrawFlags_RoundCornersRight);
        const ImVec2 text = ImGui::CalcTextSize(tab.label);
        const float left = pageRight + kTabSetBack;
        list.AddText({left + (right - left - text.x) / 2, top + (kTabHeight - text.y) / 2},
                     Scaled(kInk, 1, enabled ? 1.0f : 0.4f), tab.label);
        if (pointed && !enabled) {
            ImGui::SetTooltip("No Board: no repairs");
        }
    }
    ImGui::PopFont();
}

void ManualWindows::datasheets(const Manuals& manuals, BoardView& board,
                               const LoadedCircuit& design) {
    const BoardScene* scene = board.scene();
    if (const auto chip = scene ? board.chip() : std::nullopt) {
        ImGui::PushID("chip");
        if (ImGui::SmallButton("All datasheets")) {
            board.deselect();
            binder_.closePart();
        }
        ImGui::Spacing();
        board.route(chip_.draw(*scene, *chip, manuals, design));
        ImGui::PopID();
        return;
    }
    if (const auto& part = binder_.part()) {
        ImGui::PushID("sheet");
        if (ImGui::SmallButton("All datasheets")) {
            binder_.closePart();
        }
        ImGui::Spacing();
        if (const auto sheet = manuals.datasheets.find(*part); sheet != manuals.datasheets.end()) {
            board.route(sheet_.draw(sheet->second.blocks, scene, design, LinkScope::Parts));
        }
        ImGui::PopID();
        return;
    }
    ImGui::PushID("index");
    board.route(index(manuals, scene));
    ImGui::PopID();
}

TargetHits ManualWindows::index(const Manuals& manuals, const BoardScene* scene) {
    TargetHits hits;
    Title("Datasheets");
    ImGui::TextDisabled(scene ? "hover or click a chip, or a reference below" : "a Part's sheet");
    ImGui::Spacing();
    for (const IndexEntry& entry : index_) {
        ImGui::PushID(entry.part.c_str());
        TextFlow flow;
        const bool sheet = manuals.datasheets.contains(entry.part);
        const TextFlow::Result name = flow.text(entry.part + "  ", kPageCode, sheet);
        if (sheet && name.clicked) {
            binder_.openPart(entry.part);
        }
        for (const std::string& reference : entry.references) {
            const TextFlow::Result link =
                flow.text(reference + " ", scene ? kPageLink : kInk, scene != nullptr);
            if (!scene || (!link.hovered && !link.clicked)) {
                continue;
            }
            if (const auto component = scene->componentIndex(reference)) {
                if (link.hovered) {
                    hits.hovered = BoardTarget{{*component}, std::nullopt, std::nullopt};
                }
                if (link.clicked) {
                    hits.clicked = BoardTarget{{*component}, std::nullopt, std::nullopt};
                }
            }
        }
        ImGui::PopID();
    }
    return hits;
}

}  // namespace bw
