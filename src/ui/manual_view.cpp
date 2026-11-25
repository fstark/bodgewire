#include "ui/manual_view.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ui/touch.hpp"

namespace bw {

namespace {

// Paper colours (P6e: the manuals are pages in a binder).
constexpr ImU32 kText = IM_COL32(30, 30, 35, 255);
constexpr ImU32 kBold = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kTooltipText = IM_COL32(230, 230, 230, 255);
constexpr ImU32 kTableHeader = IM_COL32(222, 212, 182, 255);  // the cream page, darker
constexpr ImU32 kTableRule = IM_COL32(170, 160, 135, 255);
constexpr float kTitleScale = 1.3f;

ImU32 Colour(SpanStyle style) {
    switch (style) {
    case SpanStyle::Text:
        return kText;
    case SpanStyle::Code:
        return kPageCode;
    case SpanStyle::Bold:
        return kBold;
    }
    return kText;
}

void DrawSpans(std::span<const Span> spans, const BoardScene* scene, const LoadedCircuit& design,
               LinkScope scope, TargetHits& hits) {
    TextFlow flow;
    for (std::size_t i = 0; i < spans.size(); ++i) {
        const auto link = LinkAt(spans, i, design, scope);
        if (!link) {
            flow.text(spans[i].text, Colour(spans[i].style), false);
            continue;
        }
        TextFlow::Result result;
        for (std::size_t k = 0; k < link->spansUsed; ++k) {  // a run: `U1`, "–", `U5` as one
            const TextFlow::Result part = flow.text(spans[i + k].text, kPageLink, true);
            result.hovered = result.hovered || part.hovered;
            result.clicked = result.clicked || part.clicked;
        }
        i += link->spansUsed - 1;
        if (link->pinChars > 0) {
            const std::string_view next = spans[i + 1].text;
            const TextFlow::Result pin = flow.text(next.substr(0, link->pinChars), kPageLink, true);
            result.hovered = result.hovered || pin.hovered;
            result.clicked = result.clicked || pin.clicked;
            flow.text(next.substr(link->pinChars), Colour(spans[i + 1].style), false);
            ++i;
        }
        if (!scene || (!result.hovered && !result.clicked)) {
            continue;
        }
        BoardTarget target;
        if (link->net) {
            target.net = design.nets.at(*link->net);
        }
        for (const std::string& reference : link->references) {
            if (const auto component = scene->componentIndex(reference)) {
                target.components.push_back(*component);
            }
        }
        if (link->references.size() == 1 && target.components.size() == 1 && link->pin) {
            target.pad = scene->padIndex(target.components.front(), *link->pin);
        }
        if (target.components.empty() && !target.net) {
            continue;  // a part not on the Board
        }
        if (result.hovered) {
            hits.hovered = target;
        }
        if (result.clicked) {
            hits.clicked = target;
        }
    }
}

// A pipe table: the header row on a tinted band with a rule under it (no bold font is loaded),
// each cell's text wrapping at its column's edge.
void DrawTable(const Block& table, const BoardScene* scene, const LoadedCircuit& design,
               LinkScope scope, TargetHits& hits) {
    std::size_t columns = 0;
    for (const auto& row : table.rows) {
        columns = std::max(columns, row.size());
    }
    if (columns == 0) {
        return;
    }
    ImGui::PushID(static_cast<int>(table.line));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, kTableHeader);
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, kTableRule);
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, kTableRule);
    if (ImGui::BeginTable("table", static_cast<int>(columns),
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        for (std::size_t r = 0; r < table.rows.size(); ++r) {
            ImGui::TableNextRow(r == 0 ? ImGuiTableRowFlags_Headers : ImGuiTableRowFlags_None);
            if (r == 0) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kTableHeader);
            }
            for (std::size_t c = 0; c < columns; ++c) {
                ImGui::TableSetColumnIndex(static_cast<int>(c));
                if (c < table.rows[r].size()) {
                    ImGui::PushID(static_cast<int>(r * columns + c));
                    DrawSpans(table.rows[r][c], scene, design, scope, hits);
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

}  // namespace

TextFlow::TextFlow() : right_(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) {}

TextFlow::Result TextFlow::text(std::string_view raw, ImU32 colour, bool underline) {
    Result result;
    const std::string display = Displayable(raw);
    std::string_view text = display;
    while (!text.empty()) {
        std::size_t end = text.find(' ');
        end = end == std::string_view::npos ? text.size() : text.find_first_not_of(' ', end);
        end = end == std::string_view::npos ? text.size() : end;
        const std::string_view word = text.substr(0, end);
        text.remove_prefix(end);
        const float width = ImGui::CalcTextSize(word.data(), word.data() + word.size()).x;
        if (!first_) {
            ImGui::SameLine(0, 0);
            if (ImGui::GetCursorScreenPos().x + width > right_) {
                ImGui::NewLine();
            }
        }
        first_ = false;
        ImGui::PushStyleColor(ImGuiCol_Text, colour);
        ImGui::TextUnformatted(word.data(), word.data() + word.size());
        ImGui::PopStyleColor();
        const std::size_t last = word.find_last_not_of(' ');
        if (underline && last != std::string_view::npos) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float trimmed = ImGui::CalcTextSize(word.data(), word.data() + (last + 1)).x;
            ImGui::GetWindowDrawList()->AddLine({min.x, max.y}, {min.x + trimmed, max.y}, colour);
        }
        if (!underline) {
            continue;  // no link: nobody asks
        }
        // A link's own test (P8f). Its click is a release on it with no drag since the press: a
        // scroll begun on a link does not follow it. A finger's hit is 6 px wider each side and
        // as high as its line; ImGui's padding would let the words of two lines both answer.
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        if (FingerPointer()) {
            const float half = ImGui::GetStyle().ItemSpacing.y / 2;
            min = {min.x - kFingerPadding, min.y - half};
            max = {max.x + kFingerPadding, max.y + half};
        }
        const ImDrawList* list = ImGui::GetWindowDrawList();
        const ImVec2 clipMin = list->GetClipRectMin();
        const ImVec2 clipMax = list->GetClipRectMax();
        const auto inside = [&](ImVec2 at) {
            return at.x >= std::max(min.x, clipMin.x) && at.x < std::min(max.x, clipMax.x) &&
                   at.y >= std::max(min.y, clipMin.y) && at.y < std::min(max.y, clipMax.y);
        };
        const bool hovered = inside(io.MousePos) &&
                             ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        result.hovered = result.hovered || hovered;
        result.clicked = result.clicked ||
                         (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                          inside(io.MouseClickedPos[ImGuiMouseButton_Left]) && !PressCancelled() &&
                          io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
                              io.MouseDragThreshold * io.MouseDragThreshold);
    }
    return result;
}

void PageTooltip(std::string_view text) {
    if (ImGui::BeginTooltip()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTooltipText);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopStyleColor();
        ImGui::EndTooltip();
    }
}

std::string Displayable(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::string_view rest = text.substr(i);
        if (rest.starts_with("\u2013") || rest.starts_with("\u2014")) {
            result += '-';
            i += 2;
        } else if (rest.starts_with("\u2192")) {
            result += "->";
            i += 2;
        } else {
            result += text[i];
        }
    }
    return result;
}

TargetHits ManualView::draw(std::span<const Block> blocks, const BoardScene* scene,
                            const LoadedCircuit& design, LinkScope scope) {
    TargetHits hits;
    bool open = true;
    for (const Block& block : blocks) {
        switch (block.kind) {
        case BlockKind::Title: {
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
            const std::string text = Displayable(PlainText(block.spans));
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::PopFont();
            open = true;
            continue;
        }
        case BlockKind::Heading: {
            const std::string text = PlainText(block.spans);
            open = ImGui::CollapsingHeader(text.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            continue;
        }
        case BlockKind::Paragraph:
            if (open) {
                DrawSpans(block.spans, scene, design, scope, hits);
                ImGui::Spacing();
            }
            continue;
        case BlockKind::Bullet:
            if (open) {
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::BeginGroup();
                DrawSpans(block.spans, scene, design, scope, hits);
                ImGui::EndGroup();
            }
            continue;
        case BlockKind::Table:
            if (open) {
                DrawTable(block, scene, design, scope, hits);
                ImGui::Spacing();
            }
            continue;
        }
    }
    return hits;
}

}  // namespace bw
