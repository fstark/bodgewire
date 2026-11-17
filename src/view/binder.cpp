#include "view/binder.hpp"

namespace bw {

void Binder::frame(bool out, std::optional<std::size_t> selected) {
    if (out && !out_) {
        divider_ = shown_.value_or(Divider::User);
        shown_.reset();
        part_.reset();
    } else if (out && selected && selected != selected_) {
        divider_ = Divider::Datasheets;
        part_.reset();
    }
    selected_ = selected;
    out_ = out;
}

}  // namespace bw
