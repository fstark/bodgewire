#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "machine/machine.hpp"
#include "ui/board_view.hpp"
#include "view/contents_defaults.hpp"
#include "view/memory_contents.hpp"

namespace bw {

// The binder's Contents page (P6p): one ROM chip's words, in hex, as code or as glyphs, each
// chip's view and settings kept for the bench (docs/p6p-memory-contents/MEMORY_CONTENTS_DESIGN.md
// §5).
class ContentsPage {
public:
    void setDefaults(ContentsDefaults defaults) { defaults_ = std::move(defaults); }
    // The view `reference` opens at the next time it is drawn (--contents).
    void open(std::string_view reference, ContentsView view);
    // The head's link hovered, for the Board to highlight; its click frames the chip itself
    // (BoardView::focus), without selecting it, so the page stays.
    TargetHits draw(BoardView& board, std::size_t component, const Machine& machine);

private:
    struct CodeCache {
        std::string reference;
        ContentsSettings settings;
        std::vector<std::uint8_t> words;
        std::vector<CodeLine> lines;
    };

    void settingsLine(ContentsSettings& s, std::size_t words);
    void hexView(const ChipContents& chip, const ContentsSettings& s);
    void codeView(const std::string& reference, const ChipContents& chip,
                  const ContentsSettings& s);
    void glyphView(const ChipContents& chip, const ContentsSettings& s);

    ContentsDefaults defaults_;
    std::map<std::string, ContentsSettings, std::less<>> settings_;
    std::map<std::string, ContentsView, std::less<>> requested_;
    std::optional<CodeCache> code_;
    std::uint16_t goTo_ = 0;               // the Go to field's address
    std::optional<std::size_t> scrollTo_;  // the word to scroll to, once
    std::optional<std::size_t> outlined_;  // the word Go to found, outlined in the hex view
    std::string shown_;                    // the chip on the page last frame
};

}  // namespace bw
