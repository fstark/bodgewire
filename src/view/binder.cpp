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
    const BinderPlace before = here();
    if (selected && selected != selected_ && hasContents) {
        contents_ = selected;
    }
    const bool comingOut = out && !out_;
    if (comingOut) {
        divider_ = shown_.value_or(Divider::User);
        shown_.reset();
    } else if (out && selected && selected != selected_) {
        divider_ = Divider::Datasheets;
    }
    selected_ = selected;
    out_ = out;
    if (out && !comingOut && here() != before) {
        record(before);
    }
}

void Binder::turn(Divider divider) {
    if (divider != divider_) {
        record(here());
        divider_ = divider;
    }
}

BinderPlace Binder::here() const {
    switch (divider_) {
    case Divider::Datasheets:
        return {divider_, selected_};
    case Divider::Contents:
        return {divider_, contents_};
    default:
        return {divider_, std::nullopt};
    }
}

void Binder::scrolled(const BinderPlace& page, float y) { scrolls_[{page.divider, page.chip}] = y; }

std::optional<Binder::Back> Binder::back() {
    if (history_.empty()) {
        return std::nullopt;
    }
    const BinderPlace place = history_.back();
    history_.pop_back();
    divider_ = place.divider;
    if (place.divider == Divider::Datasheets) {
        selected_ = place.chip;
    } else if (place.divider == Divider::Contents) {
        contents_ = place.chip;
    }
    const auto scroll = scrolls_.find({place.divider, place.chip});
    return Back{place, scroll != scrolls_.end() ? scroll->second : 0.0f};
}

void Binder::showList() {
    const BinderPlace before = here();
    divider_ = Divider::Datasheets;
    selected_.reset();  // the caller deselects the Board: the next frame() sees no change
    if (here() != before) {
        record(before);
    }
}

void Binder::record(const BinderPlace& from) {
    if (!history_.empty() && history_.back() == from) {
        return;
    }
    history_.push_back(from);
    if (history_.size() > kHistory) {
        history_.erase(history_.begin());
    }
}

void Binder::openContents(std::size_t component) {
    const BinderPlace before = here();
    contents_ = component;
    if (out_) {
        divider_ = Divider::Contents;
        if (here() != before) {
            record(before);
        }
    } else {
        shown_ = Divider::Contents;
    }
}

}  // namespace bw
