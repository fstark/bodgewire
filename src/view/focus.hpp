#pragma once

#include <array>
#include <cstdint>

#include "view/bench_places.hpp"

namespace bw {

// What a click on a pin does: one Board tool or one instrument owns it (P6c, UX_DESIGN's Focus).
enum class Owner : std::uint8_t {
    Probe,
    Puller,
    Iron,
    Cutters,
    Wire,
    Pencil,
    Analyzer,
    Scope,
    Meter
};

bool IsInstrument(Owner owner);          // Analyzer, Scope, Meter
BenchObject ObjectOf(Owner instrument);  // its window on the bench

// The focus owner, and what is out on the bench: each object's window shown or put away, the
// instruments' and the others' (TV, keyboard, knob box, binder, brief, Debug). The probe is the
// resting state: putting the focused instrument away gives it the focus back.
class Focus {
public:
    Owner owner() const { return owner_; }
    bool out(Owner instrument) const;
    void take(Owner tool);                   // a Board tool, from the shelf
    void bringOutFocused(Owner instrument);  // out with the focus; never put away (a double-click)
    // Its shelf drawing clicked: put away when it owns the focus and its window is in front
    // (`front`); else out with the focus (the caller brings the window to the front).
    void shelfClick(Owner instrument, bool front);
    void bringOut(Owner instrument);  // start-up flags: out, the owner unchanged
    void clicked(Owner instrument);   // a click in its window: the focus, if out
    void closed(Owner instrument);    // its window closed: away; the probe takes the focus
    // Any object's out or away. Not an instrument's to put it away: closed(), for the focus.
    // A reference: a window that closes itself writes it (`draw(bool& open)`).
    bool& out(BenchObject object) { return out_[static_cast<std::size_t>(object)]; }
    bool out(BenchObject object) const { return out_[static_cast<std::size_t>(object)]; }

private:
    Owner owner_ = Owner::Probe;
    std::array<bool, kBenchObjects> out_{};  // by BenchObject
};

}  // namespace bw
