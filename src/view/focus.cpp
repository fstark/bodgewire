#include "view/focus.hpp"

#include <cassert>

#include "sim/check.hpp"

namespace bw {

namespace {

bool IsInstrument(BenchObject object) {
    return object == BenchObject::Analyzer || object == BenchObject::Scope;
}

}  // namespace

bool IsInstrument(Owner owner) { return owner == Owner::Analyzer || owner == Owner::Scope; }

bool Measures(Owner owner) {
    return owner == Owner::Probe || owner == Owner::Meter || IsInstrument(owner);
}

BenchObject ObjectOf(Owner instrument) {
    assert(IsInstrument(instrument));
    return instrument == Owner::Analyzer ? BenchObject::Analyzer : BenchObject::Scope;
}

LinkAction RouteLink(Owner owner, const BoardTarget& target, bool frame, bool secondClick) {
    const bool pin = target.components.size() == 1 && target.pad && !target.net;
    if (Measures(owner) && pin && !frame) {
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
    bringOut(instrument);
    owner_ = instrument;
}

void Focus::shelfClick(Owner instrument, bool front) {
    if (owner_ == instrument && out(instrument) && front) {
        closed(instrument);
    } else {
        bringOutFocused(instrument);
    }
}

void Focus::bringOut(Owner instrument) { bringOut(ObjectOf(instrument)); }

void Focus::bringOut(BenchObject object) { out_[static_cast<std::size_t>(object)] = true; }

void Focus::clicked(Owner instrument) {
    if (out(instrument)) {
        owner_ = instrument;
    }
}

void Focus::closed(Owner instrument) {
    out_[static_cast<std::size_t>(ObjectOf(instrument))] = false;
    if (owner_ == instrument) {
        owner_ = Owner::Probe;
    }
}

bool& Focus::outFlag(BenchObject object) {
    Check(!IsInstrument(object), "focus: an instrument is put away by closed()");
    return out_[static_cast<std::size_t>(object)];
}

}  // namespace bw
