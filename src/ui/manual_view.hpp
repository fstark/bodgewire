#pragma once

#include <span>
#include <string>
#include <string_view>

#include "manual/manual.hpp"
#include "ui/board_view.hpp"

namespace bw {

// `text` with the dashes the UI font lacks (–, —) as "-".
std::string Displayable(std::string_view text);

// Draws Markdown blocks: `##` headings collapsible, Component references as links.
class ManualView {
public:
    // Links under the mouse / clicked this frame. `scene` may be null (no Board: no targets).
    TargetHits draw(std::span<const Block> blocks, const BoardScene* scene,
                    const LoadedCircuit& design);
};

// The floating User Manual window.
class ManualWindows {
public:
    void draw(bool& open, const Manuals& manuals, BoardView& board, const LoadedCircuit& design);

private:
    ManualView user_;
};

}  // namespace bw
