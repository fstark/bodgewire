#pragma once

#include <array>
#include <cstdint>

namespace bw {

// What a click on a pin does: one Board tool or one instrument owns it (P6c, UX_DESIGN's Focus).
enum class Owner : std::uint8_t { Probe, Puller, Iron, Cutters, Wire, Analyzer, Scope, Meter };

bool IsInstrument(Owner owner);  // Analyzer, Scope, Meter

// What Esc did: cancelled a started wire, gave the probe the focus, or (the probe owning it)
// asks the caller to lift the probe's tip.
enum class EscapeStep : std::uint8_t { CancelWire, ToProbe, LiftTip };

// The focus owner and which instruments are out (their windows shown). The probe is the resting
// state: Esc and putting the focused instrument away give it the focus back.
class Focus {
public:
    Owner owner() const { return owner_; }
    bool out(Owner instrument) const;
    void take(Owner tool);       // a Board tool, from the shelf or its key
    void key(Owner instrument);  // C, O, V: out with the focus; never put away
    // Its shelf drawing clicked: put away when it owns the focus and its window is in front
    // (`front`); else out with the focus (the caller brings the window to the front).
    void shelfClick(Owner instrument, bool front);
    void bringOut(Owner instrument);  // start-up flags: out, the owner unchanged
    void clicked(Owner instrument);   // a click in its window: the focus, if out
    void closed(Owner instrument);    // its window closed: away; the probe takes the focus
    EscapeStep escape(bool wireStarted);

private:
    static std::size_t slot(Owner instrument);

    Owner owner_ = Owner::Probe;
    std::array<bool, 3> out_{};  // Analyzer, Scope, Meter
};

}  // namespace bw
