#include "view/focus.hpp"

#include <cassert>

namespace bw {

bool IsInstrument(Owner owner) {
    return owner == Owner::Analyzer || owner == Owner::Scope || owner == Owner::Meter;
}

BenchObject ObjectOf(Owner instrument) {
    assert(IsInstrument(instrument));
    return instrument == Owner::Analyzer ? BenchObject::Analyzer
           : instrument == Owner::Scope  ? BenchObject::Scope
                                         : BenchObject::Meter;
}

LinkAction RouteLink(Owner owner, const BoardTarget& target, bool frame, bool secondClick) {
    const bool measures = owner == Owner::Probe || IsInstrument(owner);
    const bool pin = target.components.size() == 1 && target.pad && !target.net;
    if (measures && pin && !frame) {
        return secondClick ? LinkAction::Frame : LinkAction::Measure;
    }
    return target.components.size() == 1 && !target.net ? LinkAction::FrameAndSelect
                                                        : LinkAction::Frame;
}

bool Focus::out(Owner instrument) const { return out(ObjectOf(instrument)); }

void Focus::take(Owner tool) {
    assert(!IsInstrument(tool));
    owner_ = tool;
}

void Focus::bringOutFocused(Owner instrument) {
    out(ObjectOf(instrument)) = true;
    owner_ = instrument;
}

void Focus::shelfClick(Owner instrument, bool front) {
    if (owner_ == instrument && out(instrument) && front) {
        closed(instrument);
    } else {
        bringOutFocused(instrument);
    }
}

void Focus::bringOut(Owner instrument) { out(ObjectOf(instrument)) = true; }

void Focus::clicked(Owner instrument) {
    if (out(instrument)) {
        owner_ = instrument;
    }
}

void Focus::closed(Owner instrument) {
    out(ObjectOf(instrument)) = false;
    if (owner_ == instrument) {
        owner_ = Owner::Probe;
    }
}

}  // namespace bw
