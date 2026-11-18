#pragma once

#include <cstdint>

namespace bw {

// Who the PC keys go to (P6g): the bench (the shelf's shortcuts, the Board's keys L, F, Tab,
// zoom, Esc) or the Machine (its keyboard, its Board keys, its knobs). Not the Focus (what a
// click on a pin does) and not ImGui's window focus.
enum class KeysTo : std::uint8_t { Bench, Machine };

class MachineKeys {
public:
    KeysTo to() const { return to_; }
    bool machine() const { return to_ == KeysTo::Machine; }
    // Each frame, before anything else: something takes keys now (the keyboard out, the knob box
    // out, Board keys with a PC key). None: the keys go back to the bench.
    void setTakers(bool takers);
    void front();   // a click on the TV, keyboard, knob box, a Board key, the shelf's keyboard
    void bench();   // a click on the Board off its keys, a tool or instrument, their windows
    void toggle();  // F2; the shelf's keyboard drawing on a Machine whose keys are on the Board

private:
    bool takers_ = false;
    KeysTo to_ = KeysTo::Bench;
};

}  // namespace bw
