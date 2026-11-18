#include "view/pencil_marks.hpp"

#include <cmath>
#include <iterator>

namespace bw {

std::optional<std::size_t> PencilMarks::nearest(Point at, std::int64_t reach) const {
    std::optional<std::size_t> best;
    double bestDistance = 0;
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        const double distance = std::hypot(static_cast<double>(marks_[i].at.x - at.x),
                                           static_cast<double>(marks_[i].at.y - at.y));
        // The last on a tie: the one drawn on top.
        if (distance <= static_cast<double>(reach) && (!best || distance <= bestDistance)) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

void PencilMarks::click(Point at, std::optional<std::size_t> body, std::int64_t reach) {
    const auto found = nearest(at, reach);
    if (!found) {
        marks_.push_back({at, MarkShape::Dot, body});
        return;
    }
    PencilMark& mark = marks_[*found];
    switch (mark.shape) {
    case MarkShape::Dot:
        mark.shape = MarkShape::Check;
        break;
    case MarkShape::Check:
        mark.shape = MarkShape::Cross;
        break;
    case MarkShape::Cross:
        marks_.erase(std::next(marks_.begin(), static_cast<std::ptrdiff_t>(*found)));
        break;
    }
}

bool PencilMarks::clear(Point at, std::int64_t reach) {
    const auto found = nearest(at, reach);
    if (found) {
        marks_.erase(std::next(marks_.begin(), static_cast<std::ptrdiff_t>(*found)));
    }
    return found.has_value();
}

void PencilMarks::swapped(std::size_t component) {
    std::erase_if(marks_, [&](const PencilMark& mark) { return mark.body == component; });
}

void PencilMarks::add(PencilMark mark) { marks_.push_back(mark); }

}  // namespace bw
