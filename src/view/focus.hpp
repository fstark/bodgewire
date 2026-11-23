#pragma once

#include <array>
#include <cstdint>

#include "view/bench_places.hpp"
#include "view/board_scene.hpp"

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

// What a click on a link to the Board (a manual's or the chip panel's pin, part or Net) does
// (P6i, P6o): under the probe or an instrument a pin is measured, as if clicked on the Board;
// with Cmd or Ctrl (`frame`), or for anything but a pin, the Board frames it and a single part is
// selected. A double-click's second click frames what the first measured.
enum class LinkAction : std::uint8_t { Measure, Frame, FrameAndSelect };
LinkAction RouteLink(Owner owner, const BoardTarget& target, bool frame, bool secondClick);

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
    void bringOut(Owner instrument);    // start-up flags: out, the owner unchanged
    void bringOut(BenchObject object);  // the same for any object (--pop)
    void clicked(Owner instrument);     // a click in its window: the focus, if out
    void closed(Owner instrument);      // its window closed: away; the probe takes the focus
    bool out(BenchObject object) const { return out_[static_cast<std::size_t>(object)]; }
    // Out or away for an object that is no instrument (TV, keyboard, knob box, binder, brief,
    // Debug), written by what shows it and by its window when it closes (`draw(bool& open)`).
    // Refuses an instrument: that one comes out and goes away above, for the focus.
    bool& outFlag(BenchObject object);

private:
    Owner owner_ = Owner::Probe;
    std::array<bool, kBenchObjects> out_{};  // by BenchObject
};

}  // namespace bw
