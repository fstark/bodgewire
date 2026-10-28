#include "ui/manual_view.hpp"

#include <string>
#include <string_view>

#include <imgui.h>

namespace bw {

namespace {

constexpr ImU32 kText = IM_COL32(225, 225, 225, 255);
constexpr ImU32 kBold = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kCode = IM_COL32(235, 200, 120, 255);
constexpr ImU32 kLink = IM_COL32(110, 185, 255, 255);
constexpr float kTitleScale = 1.3f;
constexpr std::string_view kTroubleshooting = "Troubleshooting";

// Lays out words left to right, wrapping at the right edge of the content region.
class Flow {
public:
    Flow() : right_(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) {}

    // Each word keeps its trailing spaces. True when the mouse is over one of them.
    struct Result {
        bool hovered = false, clicked = false;
    };
    Result text(std::string_view raw, ImU32 colour, bool underline) {
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
                ImGui::GetWindowDrawList()->AddLine({min.x, max.y}, {min.x + trimmed, max.y},
                                                    colour);
            }
            result.hovered = result.hovered || ImGui::IsItemHovered();
            result.clicked = result.clicked || ImGui::IsItemClicked(ImGuiMouseButton_Left);
        }
        return result;
    }

private:
    float right_;
    bool first_ = true;
};

ImU32 Colour(SpanStyle style) {
    switch (style) {
    case SpanStyle::Text:
        return kText;
    case SpanStyle::Code:
        return kCode;
    case SpanStyle::Bold:
        return kBold;
    }
    return kText;
}

void DrawSpans(std::span<const Span> spans, const BoardScene* scene, const LoadedCircuit& design,
               TargetHits& hits) {
    Flow flow;
    for (std::size_t i = 0; i < spans.size(); ++i) {
        const auto link = LinkAt(spans, i, design);
        if (!link) {
            flow.text(spans[i].text, Colour(spans[i].style), false);
            continue;
        }
        Flow::Result result = flow.text(spans[i].text, kLink, true);
        if (link->pinChars > 0) {
            const std::string_view next = spans[i + 1].text;
            const Flow::Result pin = flow.text(next.substr(0, link->pinChars), kLink, true);
            result.hovered = result.hovered || pin.hovered;
            result.clicked = result.clicked || pin.clicked;
            flow.text(next.substr(link->pinChars), Colour(spans[i + 1].style), false);
            ++i;
        }
        if (!scene || (!result.hovered && !result.clicked)) {
            continue;
        }
        const auto component = scene->componentIndex(link->reference);
        if (!component) {
            continue;
        }
        BoardTarget target{*component, std::nullopt};
        if (link->pin) {
            target.pad = scene->padIndex(*component, *link->pin);
        }
        if (result.hovered) {
            hits.hovered = target;
        }
        if (result.clicked) {
            hits.clicked = target;
        }
    }
}

}  // namespace

std::string Displayable(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::string_view rest = text.substr(i);
        if (rest.starts_with("\u2013") || rest.starts_with("\u2014")) {
            result += '-';
            i += 2;
        } else {
            result += text[i];
        }
    }
    return result;
}

TargetHits ManualView::draw(std::span<const Block> blocks, const BoardScene* scene,
                            const LoadedCircuit& design) {
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
            const bool trouble = text == kTroubleshooting;
            const std::string label = trouble ? text + " (easy mode)" : text;
            open = ImGui::CollapsingHeader(label.c_str(),
                                           trouble ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
            continue;
        }
        case BlockKind::Paragraph:
            if (open) {
                DrawSpans(block.spans, scene, design, hits);
                ImGui::Spacing();
            }
            continue;
        case BlockKind::Bullet:
            if (open) {
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::BeginGroup();
                DrawSpans(block.spans, scene, design, hits);
                ImGui::EndGroup();
            }
            continue;
        }
    }
    return hits;
}

void ManualWindows::draw(bool& userOpen, bool& serviceOpen, const Manuals& manuals,
                         BoardView& board, const LoadedCircuit& design) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const auto window = [&](const char* name, bool& open, const std::optional<Markdown>& doc,
                            const char* missing, ManualView& view, float x) {
        if (!open) {
            return;
        }
        ImGui::SetNextWindowSize({480, 600}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({viewport->WorkPos.x + x, viewport->WorkPos.y + 120},
                                ImGuiCond_FirstUseEver);
        if (ImGui::Begin(name, &open, ImGuiWindowFlags_NoSavedSettings)) {
            if (doc) {
                board.route(view.draw(doc->blocks, board.scene(), design));
            } else {
                ImGui::TextDisabled("%s", missing);
            }
        }
        ImGui::End();
    };
    window("User Manual", userOpen, manuals.user, "no user manual", user_, 40);
    window("Service Manual", serviceOpen, manuals.service, "no service manual", service_, 540);
}

}  // namespace bw
