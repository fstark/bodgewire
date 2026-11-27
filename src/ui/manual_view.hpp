#pragma once

#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"

namespace bw {

// `text` with the dashes the UI font lacks (–, —) as "-".
std::string Displayable(std::string_view text);

// Paper colours a page shares with its own drawings (P6e): code, links.
inline constexpr ImU32 kPageCode = IM_COL32(150, 90, 10, 255);
inline constexpr ImU32 kPageLink = IM_COL32(25, 80, 175, 255);

// Lays out words left to right, wrapping at the right edge of the content region.
class TextFlow {
public:
    TextFlow();
    // Each word keeps its trailing spaces. With `underline` (a link): whether the pointer is over
    // one of them / was released on one with no drag since its press (P8f); `dotted`: the
    // underline in 2 px dashes (a chip's pin in its sheet, P8j).
    struct Result {
        bool hovered = false, clicked = false;
        bool held = false;  // pressed on it, not yet let go nor dragged (P8g)
    };
    Result text(std::string_view raw, ImU32 colour, bool underline, bool dotted = false);

private:
    float right_;
    bool first_ = true;
};

// A tooltip from a page: the UI's light text on its dark background and its own padding,
// whatever the page pushed.
void PageTooltip(std::string_view text);

// One paragraph's spans flowed and wrapped, its links drawn and tested (`chip`: the Chip scope's
// reference, P8j). Links under the mouse / clicked this frame; `scene` may be null.
TargetHits FlowSpans(std::span<const Span> spans, const BoardScene* scene,
                     const LoadedCircuit& design, LinkScope scope, std::string_view chip = {});

// Draws Markdown blocks: `##` headings collapsible, links by `scope` (`chip`: the Chip scope's
// reference).
class ManualView {
public:
    // Links under the mouse / clicked this frame. `scene` may be null (no Board: no targets).
    TargetHits draw(std::span<const Block> blocks, const BoardScene* scene,
                    const LoadedCircuit& design, LinkScope scope, std::string_view chip = {});
};

}  // namespace bw
