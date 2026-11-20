#include "ui/contents_page.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include <imgui.h>

#include "ui/manual_view.hpp"

namespace bw {

namespace {

constexpr float kTitleScale = 1.3f;
constexpr float kLabelScale = 0.8f;
constexpr ImU32 kPaper = IM_COL32(246, 242, 228, 255);  // a combo's list: a slip of paper
constexpr ImU32 kGlyphBack = IM_COL32(30, 28, 26, 255);
constexpr ImU32 kGlyphLit = IM_COL32(255, 200, 80, 255);  // a lit phosphor on the paper
constexpr ImU32 kOutline = kPageLink;
constexpr ImU32 kMargin = IM_COL32(120, 115, 105, 255);  // addresses and bytes: the margin's ink

// "£" as the font draws it: a dot when it has no glyph for it.
std::string Drawable(std::string line) {
    static const bool pound = ImGui::GetFont()->IsGlyphInFont(0xA3);
    if (!pound) {
        for (std::size_t i = line.find("\xC2\xA3"); i != std::string::npos;
             i = line.find("\xC2\xA3", i)) {
            line.replace(i, 2, ".");
        }
    }
    return line;
}

// Lays the settings' items left to right, wrapping when the next one does not fit.
class Row {
public:
    Row() : right_(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) {}
    // Before an item of `width` px.
    void next(float width) {
        if (!first_) {
            ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2);
            if (ImGui::GetCursorScreenPos().x + width > right_) {
                ImGui::NewLine();
            }
        }
        first_ = false;
    }

private:
    float right_;
    bool first_ = true;
};

constexpr float kHexField = 46, kSlider = 64;

// "Label [field]": an InputScalar in hex; true when edited (or Enter, with EnterReturnsTrue).
template <typename T>
bool HexField(Row& row, const char* label, const char* id, T& value, const char* format,
              ImGuiInputTextFlags flags = 0) {
    const ImGuiDataType type = sizeof(T) == 2 ? ImGuiDataType_U16 : ImGuiDataType_U64;
    row.next(ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x + kHexField);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(kHexField);
    return ImGui::InputScalar(id, type, &value, nullptr, nullptr, format,
                              ImGuiInputTextFlags_CharsHexadecimal | flags);
}

bool Combo(Row& row, const char* label, const char* id, int& value, const char* items,
           float width) {
    row.next(ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x + width);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kPaper);
    const bool changed = ImGui::Combo(id, &value, items);
    ImGui::PopStyleColor();
    return changed;
}

bool Slider(Row& row, const char* label, const char* id, unsigned& value, int low, int high) {
    row.next(ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x + kSlider);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(kSlider);
    int v = static_cast<int>(value);
    const bool changed = ImGui::SliderInt(id, &v, low, high);
    value = static_cast<unsigned>(std::clamp(v, low, high));
    return changed;
}

}  // namespace

void ContentsPage::open(std::string_view reference, ContentsView view) {
    requested_[std::string(reference)] = view;
}

TargetHits ContentsPage::draw(BoardView& board, std::size_t component, const Machine& machine) {
    TargetHits hits;
    const BoardScene& scene = *board.scene();
    const std::string& reference = scene.board().components[component].reference;
    const auto chip = machine.contents(reference);
    if (!chip) {
        ImGui::TextDisabled("no contents");
        return hits;
    }
    const std::size_t words = chip->words.size();
    ContentsSettings& s =
        settings_.try_emplace(reference, DefaultSettings(defaults_, reference, chip->bits))
            .first->second;
    if (reference != shown_) {
        shown_ = reference;
        outlined_.reset();
        scrollTo_.reset();
        goTo_ = s.address;  // Go to starts at the chip's first word
    }
    if (const auto r = requested_.find(reference); r != requested_.end()) {
        s.view = r->second;
        requested_.erase(r);
    }

    // The head: the reference (a link), the Part, its size, where the CPU sees it.
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleScale);
    TextFlow flow;
    const TextFlow::Result link = flow.text(reference, kPageLink, true);
    ImGui::PopFont();
    const BoardTarget target{{component}, std::nullopt, std::nullopt};
    if (link.hovered) {
        hits.hovered = target;
    }
    if (link.clicked) {
        board.focus(target);  // not route(): it would select the chip and turn to Datasheets
    }
    const auto own = defaults_.chips.find(reference);
    const bool mapped = (own != defaults_.chips.end() && own->second.address) || s.address != 0;
    std::string about =
        std::format("  {}  {}", scene.partName(component), SizeText(words, chip->bits));
    if (mapped) {
        about += std::format("  ${:04X}-${:04X}", s.address, (s.address + words - 1) & 0xFFFFu);
    }
    ImGui::SameLine(0, 0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(about.c_str());

    // The three views.
    constexpr std::array<std::pair<ContentsView, const char*>, 3> kViews{
        {{ContentsView::Hex, "Hex"},
         {ContentsView::Code, "Code"},
         {ContentsView::Glyphs, "Glyphs"}}};
    for (std::size_t i = 0; i < kViews.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine();
        }
        const bool on = s.view == kViews[i].first;
        if (on) {  // the open view: pressed in, framed in ink
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Text));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        }
        if (ImGui::SmallButton(kViews[i].second)) {
            s.view = kViews[i].first;
        }
        if (on) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }
    settingsLine(s, words);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::BeginChild("view", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
        switch (s.view) {
        case ContentsView::Hex:
            hexView(*chip, s);
            break;
        case ContentsView::Code:
            codeView(reference, *chip, s);
            break;
        case ContentsView::Glyphs:
            glyphView(*chip, s);
            break;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    return hits;
}

void ContentsPage::settingsLine(ContentsSettings& s, std::size_t words) {
    Row row;
    if (s.view != ContentsView::Glyphs) {  // glyphs count the chip's own words
        HexField(row, "Address", "##address", s.address, "%04X");
    }
    bool goTo = false;
    switch (s.view) {
    case ContentsView::Hex: {
        int set = static_cast<int>(s.charset);
        if (Combo(row, "Characters", "##charset", set, "ASCII\0PETSCII\0ZX80\0", 76)) {
            s.charset = static_cast<CharacterSet>(set);
        }
        goTo =
            HexField(row, "Go to", "##goto", goTo_, "%04X", ImGuiInputTextFlags_EnterReturnsTrue);
        break;
    }
    case ContentsView::Code: {
        int cpu = static_cast<int>(s.cpu);
        if (Combo(row, "CPU", "##cpu", cpu, "6502\0Z80\0", 60)) {
            s.cpu = static_cast<Cpu>(cpu);
        }
        std::uint64_t from = s.codeFrom;
        if (HexField(row, "From", "##from", from, "%03llX")) {
            s.codeFrom = std::min<std::size_t>(from, words - 1);
        }
        std::uint64_t length = s.codeLength != 0 ? s.codeLength : words - s.codeFrom;
        if (HexField(row, "Length", "##length", length, "%03llX")) {
            s.codeLength = length;
        }
        goTo =
            HexField(row, "Go to", "##goto", goTo_, "%04X", ImGuiInputTextFlags_EnterReturnsTrue);
        break;
    }
    case ContentsView::Glyphs: {
        std::uint64_t from = s.glyphFrom;
        if (HexField(row, "From", "##from", from, "%03llX")) {
            s.glyphFrom = std::min<std::size_t>(from, words - 1);
        }
        Slider(row, "Rows", "##rows", s.glyphRows, 1, 16);
        Slider(row, "Bits", "##bits", s.glyphBits, 1, 8);
        break;
    }
    }
    if (goTo) {
        // An address outside the chip does nothing.
        const std::size_t offset = (goTo_ - s.address) & 0xFFFFu;
        if (offset < words) {
            scrollTo_ = offset;
            outlined_ = offset;
        }
    }
}

void ContentsPage::hexView(const ChipContents& chip, const ContentsSettings& s) {
    const float digit = ImGui::CalcTextSize("0").x;
    const int columns =
        HexColumns((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize) / digit);
    const auto perRow = static_cast<std::size_t>(columns);
    const std::size_t rows = (chip.words.size() + perRow - 1) / perRow;
    const float line = ImGui::GetTextLineHeightWithSpacing();
    if (scrollTo_) {
        ImGui::SetScrollY(static_cast<float>(*scrollTo_ / perRow) * line);
        scrollTo_.reset();
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows), line);
    while (clipper.Step()) {
        for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
            const std::size_t from = static_cast<std::size_t>(r) * perRow;
            const std::string text =
                Drawable(HexLine(chip.words, s.address, from, columns, s.charset));
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::TextUnformatted(text.c_str());
            if (outlined_ && *outlined_ >= from && *outlined_ < from + perRow) {
                // "AAAA  " then "WW " a word: measured, whatever the font's widths.
                const std::size_t start = 6 + 3 * (*outlined_ - from);
                const float x0 = at.x + ImGui::CalcTextSize(text.c_str(), text.c_str() + start).x;
                const float x1 =
                    at.x + ImGui::CalcTextSize(text.c_str(), text.c_str() + start + 2).x;
                ImGui::GetWindowDrawList()->AddRect({x0 - 2, at.y - 1},
                                                    {x1 + 2, at.y + ImGui::GetTextLineHeight() + 1},
                                                    kOutline, 2.0f);
            }
        }
    }
}

void ContentsPage::codeView(const std::string& reference, const ChipContents& chip,
                            const ContentsSettings& s) {
    if (!code_ || code_->reference != reference || code_->settings != s ||
        code_->words != chip.words) {
        code_ = CodeCache{reference, s, chip.words, Disassembly(chip.words, s)};
    }
    const std::vector<CodeLine>& lines = code_->lines;
    const float line = ImGui::GetTextLineHeightWithSpacing();
    std::optional<std::size_t> found;  // the line Go to found
    if (outlined_) {
        const auto it = std::ranges::upper_bound(lines, *outlined_, {}, &CodeLine::offset);
        if (it != lines.begin()) {
            found = static_cast<std::size_t>(it - lines.begin()) - 1;
        }
    }
    if (scrollTo_) {
        if (found) {
            ImGui::SetScrollY(static_cast<float>(*found) * line);
        }
        scrollTo_.reset();
    }
    const float byteWidth = ImGui::CalcTextSize("00 ").x;
    const float bytesAt = ImGui::CalcTextSize("0000  ").x;
    const float textAt = bytesAt + byteWidth * (s.cpu == Cpu::Mos6502 ? 3.0f : 4.0f) + byteWidth;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(lines.size()), line);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const CodeLine& l = lines[static_cast<std::size_t>(i)];
            std::string bytes;
            for (std::size_t b = 0; b < l.length; ++b) {
                bytes += std::format("{:02X} ", chip.words[l.offset + b]);
            }
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float left = ImGui::GetCursorPosX();
            ImGui::PushStyleColor(ImGuiCol_Text, kMargin);
            ImGui::TextUnformatted(std::format("{:04X}", l.address).c_str());
            ImGui::SameLine(left + bytesAt);
            ImGui::TextUnformatted(bytes.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(left + textAt);
            ImGui::TextUnformatted(l.text.c_str());
            if (found == static_cast<std::size_t>(i)) {
                ImGui::GetWindowDrawList()->AddRect(
                    {at.x - 2, at.y - 1},
                    {ImGui::GetItemRectMax().x + 2, at.y + ImGui::GetTextLineHeight() + 1},
                    kOutline, 2.0f);
            }
        }
    }
}

void ContentsPage::glyphView(const ChipContents& chip, const ContentsSettings& s) {
    const float width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
    const GlyphGrid grid = GlyphLayout(width, s.glyphBits);
    const float dot = grid.dot;
    const float labelSize = ImGui::GetStyle().FontSizeBase * kLabelScale;
    const float cellW = dot * static_cast<float>(s.glyphBits + 2);
    const float cellH = dot * static_cast<float>(s.glyphRows + 1) + labelSize + 2;
    const std::size_t count = GlyphCount(chip.words.size(), s);
    const auto columns = static_cast<std::size_t>(grid.columns);
    const std::size_t rows = (count + columns - 1) / columns;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows), cellH);
    while (clipper.Step()) {
        for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            for (std::size_t c = 0; c < columns; ++c) {
                const std::size_t g = static_cast<std::size_t>(r) * columns + c;
                if (g >= count) {
                    break;
                }
                const ImVec2 at{origin.x + static_cast<float>(c) * cellW, origin.y};
                const ImVec2 size{dot * static_cast<float>(s.glyphBits),
                                  dot * static_cast<float>(s.glyphRows)};
                list.AddRectFilled(at, {at.x + size.x, at.y + size.y}, kGlyphBack);
                const float inset = dot >= 4 ? 0.5f : 0.0f;  // a hairline between dots
                for (unsigned y = 0; y < s.glyphRows; ++y) {
                    for (unsigned x = 0; x < s.glyphBits; ++x) {
                        if (GlyphDot(chip.words, s, g, y, x)) {
                            const ImVec2 p{at.x + static_cast<float>(x) * dot,
                                           at.y + static_cast<float>(y) * dot};
                            list.AddRectFilled({p.x + inset, p.y + inset},
                                               {p.x + dot - inset, p.y + dot - inset}, kGlyphLit);
                        }
                    }
                }
                list.AddText(nullptr, labelSize, {at.x, at.y + size.y + 2},
                             ImGui::GetColorU32(ImGuiCol_Text), std::format("{:02X}", g).c_str());
                if (ImGui::IsWindowHovered() &&
                    ImGui::IsMouseHoveringRect(at, {at.x + cellW, at.y + cellH})) {
                    const std::size_t first = s.glyphFrom + g * s.glyphRows;
                    const std::size_t last = std::min(first + s.glyphRows, chip.words.size()) - 1;
                    PageTooltip(std::format("${:02X}: words ${:03X}-${:03X}", g, first, last));
                }
            }
            ImGui::Dummy({width, cellH});
        }
    }
}

}  // namespace bw
