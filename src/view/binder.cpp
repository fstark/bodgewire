#include "view/binder.hpp"

#include <algorithm>

namespace bw {

std::vector<float> TabTops(std::size_t count, float top, float bottom, float tabHeight,
                           float step) {
    if (count > 1) {
        const float room = bottom - top - tabHeight;  // for the tops after the first
        step = std::clamp(room / static_cast<float>(count - 1), 0.0f, step);
    }
    std::vector<float> tops(count);
    for (std::size_t i = 0; i < count; ++i) {
        tops[i] = top + step * static_cast<float>(i);
    }
    return tops;
}

void Binder::frame(bool out, std::optional<std::size_t> selected, bool hasContents) {
    if (selected && selected != selected_ && hasContents) {
        contents_ = selected;
    }
    if (out && !out_) {
        divider_ = shown_.value_or(Divider::User);
        shown_.reset();
    } else if (out && selected && selected != selected_) {
        divider_ = Divider::Datasheets;
    }
    selected_ = selected;
    out_ = out;
}

void Binder::openContents(std::size_t component) {
    contents_ = component;
    if (out_) {
        divider_ = Divider::Contents;
    } else {
        shown_ = Divider::Contents;
    }
}

}  // namespace bw
