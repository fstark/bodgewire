#include "tv_window.hpp"

#include <algorithm>

#include "view/tv_controls.hpp"

namespace bw {

std::optional<LitBox> LitBounds(const TvFrame& frame, int fromRow, int toRow) {
    std::optional<LitBox> box;
    for (int row = fromRow; row < toRow; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            if (frame.at(row, column) < 0.5f) {
                continue;
            }
            if (!box) {
                box = LitBox{row, row, column, column};
            }
            box->bottom = row;
            box->left = std::min(box->left, column);
            box->right = std::max(box->right, column);
        }
    }
    return box;
}

bool InsideWindow(const LitBox& box, int lines) {
    return box.left >= static_cast<int>(kWindowLeft) &&
           box.right < static_cast<int>(kWindowRight) && box.top >= static_cast<int>(kWindowTop) &&
           box.bottom < lines + 1;
}

}  // namespace bw
