#include "ui/manual_view.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include <imgui.h>

#include "text/text.hpp"
#include "ui/page_fonts.hpp"
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

// A span in its own face: bold and code in theirs, plain text in the one around it (a table's
// header row: bold).
TextFlow::Result FlowSpan(TextFlow& flow, std::string_view text, SpanStyle style, ImU32 colour,
                          bool underline, bool dotted = false) {
    if (style == SpanStyle::Text) {
        return flow.text(text, colour, underline, dotted);
    }
    PushPageFont(style == SpanStyle::Bold ? PageFace::Bold : PageFace::Mono);
    const TextFlow::Result result = flow.text(text, colour, underline, dotted);
    ImGui::PopFont();
    return result;
}

void DrawSpans(std::span<const Span> spans, const BoardScene* scene, const LoadedCircuit& design,
               LinkScope scope, std::string_view chip, TargetHits& hits) {
    TextFlow flow;
    // A chip's pins (P8j) keep the code's colour and face, a dotted underline: a pin, not a part.
    const bool pins = scope == LinkScope::Chip;
    for (std::size_t i = 0; i < spans.size(); ++i) {
        const auto link = LinkAt(spans, i, design, scope, chip);
        if (!link) {
            FlowSpan(flow, spans[i].text, spans[i].style, Colour(spans[i].style), false);
            continue;
        }
        TextFlow::Result result;
        for (std::size_t k = 0; k < link->spansUsed; ++k) {  // a run: `U1`, "–", `U5` as one
            const SpanStyle style = spans[i + k].style;
            const TextFlow::Result part =
                FlowSpan(flow, spans[i + k].text, style, pins ? Colour(style) : kPageLink,
                         !pins || style == SpanStyle::Code, pins);
            result.hovered = result.hovered || part.hovered;
            result.clicked = result.clicked || part.clicked;
            result.held = result.held || part.held;
        }
        i += link->spansUsed - 1;
        if (link->pinChars > 0) {
            const std::string_view next = spans[i + 1].text;
            const SpanStyle style = spans[i + 1].style;
            const TextFlow::Result pin =
                FlowSpan(flow, next.substr(0, link->pinChars), style, kPageLink, true);
            result.hovered = result.hovered || pin.hovered;
            result.clicked = result.clicked || pin.clicked;
            result.held = result.held || pin.held;
            FlowSpan(flow, next.substr(link->pinChars), style, Colour(style), false);
            ++i;
        }
        if (!scene || (!result.hovered && !result.clicked && !result.held)) {
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
        if (link->references.size() == 1 && target.components.size() == 1 &&
            link->pins.size() > 1) {  // a range of the chip's pins: its pads, no one pad
            for (const std::string& number : link->pins) {
                if (const auto pad = scene->padIndex(target.components.front(), number)) {
                    target.pads.push_back(*pad);
                }
            }
        } else if (link->references.size() == 1 && target.components.size() == 1 && link->pin) {
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
        if (result.held) {
            hits.held = target;
        }
    }
}

// A pipe table: the header row in bold on a tinted band with a rule under it, each cell's text
// wrapping at its column's edge.
void DrawTable(const Block& table, const BoardScene* scene, const LoadedCircuit& design,
               LinkScope scope, std::string_view chip, TargetHits& hits) {
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
                    if (r == 0) {
                        PushPageFont(PageFace::Bold);
                    }
                    DrawSpans(table.rows[r][c], scene, design, scope, chip, hits);
                    if (r == 0) {
                        ImGui::PopFont();
                    }
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

TextFlow::Result TextFlow::text(std::string_view raw, ImU32 colour, bool underline, bool dotted) {
    Result result;
    const std::string display = Displayable(raw);
    std::string_view text = display;
    while (!text.empty()) {
        std::size_t end = text.find(' ');
        while (end != std::string_view::npos && end > 0 && end + 1 < text.size() &&
               IsDigit(text[end - 1]) && IsDigit(text[end + 1])) {
            end = text.find(' ', end + 1);  // a number's groups stay on one line: 1 048 576
        }
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
            ImDrawList& list = *ImGui::GetWindowDrawList();
            if (!dotted) {
                list.AddLine({min.x, max.y}, {min.x + trimmed, max.y}, colour);
            }
            for (float x = min.x; dotted && x < min.x + trimmed; x += 4) {  // 2 px on, 2 off
                list.AddLine({x, max.y}, {std::min(x + 2, min.x + trimmed), max.y}, colour);
            }
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
        const bool still = hovered && inside(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
                           !PressCancelled() &&
                           io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
                               io.MouseDragThreshold * io.MouseDragThreshold;
        result.clicked = result.clicked || (still && ImGui::IsMouseReleased(ImGuiMouseButton_Left));
        result.held = result.held || (still && ImGui::IsMouseDown(ImGuiMouseButton_Left));
    }
    return result;
}

void PageTooltip(std::string_view text) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 6});  // a page pushes none
    const bool open = ImGui::BeginTooltip();
    ImGui::PopStyleVar();
    if (open) {
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

TargetHits FlowSpans(std::span<const Span> spans, const BoardScene* scene,
                     const LoadedCircuit& design, LinkScope scope, std::string_view chip) {
    TargetHits hits;
    DrawSpans(spans, scene, design, scope, chip, hits);
    return hits;
}

TargetHits ManualView::draw(std::span<const Block> blocks, const BoardScene* scene,
                            const LoadedCircuit& design, LinkScope scope, std::string_view chip) {
    TargetHits hits;
    bool open = true;
    for (const Block& block : blocks) {
        switch (block.kind) {
        case BlockKind::Title: {
            PushPageFont(PageFace::Bold, kTitleScale);
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
                DrawSpans(block.spans, scene, design, scope, chip, hits);
                ImGui::Spacing();
            }
            continue;
        case BlockKind::Bullet:
            if (open) {
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::BeginGroup();
                DrawSpans(block.spans, scene, design, scope, chip, hits);
                ImGui::EndGroup();
            }
            continue;
        case BlockKind::Table:
            if (open) {
                DrawTable(block, scene, design, scope, chip, hits);
                ImGui::Spacing();
            }
            continue;
        }
    }
    return hits;
}

}  // namespace bw
