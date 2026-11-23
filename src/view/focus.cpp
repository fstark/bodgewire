#include "view/focus.hpp"

#include <cassert>

namespace bw {

bool IsInstrument(Owner owner) {
    return owner == Owner::Analyzer || owner == Owner::Scope || owner == Owner::Meter;
}

std::size_t Focus::slot(Owner instrument) {
    assert(IsInstrument(instrument));
    return static_cast<std::size_t>(instrument) - static_cast<std::size_t>(Owner::Analyzer);
}

bool Focus::out(Owner instrument) const { return out_[slot(instrument)]; }

void Focus::take(Owner tool) {
    assert(!IsInstrument(tool));
    owner_ = tool;
}

void Focus::bringOutFocused(Owner instrument) {
    out_[slot(instrument)] = true;
    owner_ = instrument;
}

void Focus::shelfClick(Owner instrument, bool front) {
    if (owner_ == instrument && out(instrument) && front) {
        closed(instrument);
    } else {
        bringOutFocused(instrument);
    }
}

void Focus::bringOut(Owner instrument) { out_[slot(instrument)] = true; }

void Focus::clicked(Owner instrument) {
    if (out(instrument)) {
        owner_ = instrument;
    }
}

void Focus::closed(Owner instrument) {
    out_[slot(instrument)] = false;
    if (owner_ == instrument) {
        owner_ = Owner::Probe;
    }
}

}  // namespace bw
