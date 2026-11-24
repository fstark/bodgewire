#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "parts/key_matrix.hpp"
#include "session/leads.hpp"

namespace bw {

struct SwitchKey {
    std::string reference;
    std::string pc;  // IsPcKey
};

// The Machine's input devices as the player works them (Session::inputs()): its switches
// (BUTTON, KEY, SLIDE), knobs (PADDLE), KEYBOARD and key matrices. What is held is released at
// power-off; the knobs' turns are kept across power-off, power-on and rebuilds. A switch, a knob
// or a held key reaches its Part through Runner::post (the simulation thread's); typing goes
// through each keyboard's own queue, which answers at once whether it took the key.
class InputDevices {
public:
    explicit InputDevices(const SessionBuild& build) : build_(&build) {}

    // Every switch (BUTTON, KEY, SLIDE) of the circuit, in netlist order.
    std::vector<std::string> switches() const;
    // Switches with a PC key (`pc=`), in netlist order.
    std::vector<SwitchKey> switchKeys() const;
    // Ignored while not powered. Applied at the next slice boundary; power-off releases all.
    void setPressed(std::string_view reference, bool pressed);
    bool pressed(std::string_view reference) const { return pressed_.contains(reference); }

    // Every PADDLE Part of the circuit, in netlist order.
    std::vector<std::string> knobs() const;
    // Clamped to 0..1; any time, powered or not; kept across power-off, power-on and rebuilds.
    // False (and nothing set) for a reference that is not a PADDLE.
    bool setKnob(std::string_view reference, double turn);
    double knob(std::string_view reference) const;  // 0 until set

    // Every KEYBOARD Part of the circuit, in netlist order.
    std::vector<std::string> keyboards() const;
    // Sends `code` (0x00-kKeyboardLastCode: control codes from CTRL, ESC, RETURN, BACK SPACE,
    // and the upper-case set) to KEYBOARD `reference`, which takes it at the next slice boundary.
    // False, nothing sent: not powered, not a KEYBOARD, a code it does not send, or
    // kKeyboardWaiting keys already waiting. Keys still waiting at power-off are dropped.
    bool typeCode(std::string_view reference, std::uint8_t code);
    // typeCode with the character's KeyboardCode (text, --type); on a key matrix, the press that
    // types `key` (its key map, kKey… for the named keys), queued in simulation time (held, then
    // released). False, nothing sent: not powered, no such keyboard, no key for it, queue full.
    bool type(std::string_view reference, char32_t key);
    // `text` typed: on a key matrix as a whole (the BW-80's keyboard reads keywords out of it:
    // Zx80Presses), on a KEYBOARD one character at a time. False when nothing was sent.
    bool typeText(std::string_view reference, std::u32string_view text);
    // One press on key matrix `reference` as it is (a key, SHIFT with it or not), queued in
    // simulation time as type's are. False when nothing was sent.
    bool press(std::string_view reference, MatrixPress press);

    // Every key matrix Part (BW2001KB, the BW-2001 keyboard, P5q; BW80KB, the BW-80's, P5x),
    // in netlist order.
    std::vector<std::string> matrixKeyboards() const;
    // Which keyboard `reference` is (its look and key map); nullopt for anything else.
    std::optional<KeyMatrixKind> matrixKind(std::string_view reference) const;
    // Holds or releases key `key` (index in the Part's key table) of key matrix `reference`;
    // ignored while unpowered; every held key is released at power-off, as switch presses are.
    void setMatrixKeyHeld(std::string_view reference, std::size_t key, bool held);
    bool matrixKeyHeld(std::string_view reference, std::size_t key) const;
    // Held, or pressed now by typing (type, --type, paste): drawn down.
    bool matrixKeyDown(std::string_view reference, std::size_t key) const;
    // The codes sent to `reference` since power-on, the last kTypedShown.
    std::string typed(std::string_view reference) const;
    static constexpr std::size_t kTypedShown = 16;

private:
    friend class Session;  // its builds and its power
    void bind();           // a new build: the knobs' turns applied to it
    void poweredOn();      // the codes typed forgotten
    void poweredOff();     // every switch and key released, the keys waiting to be typed dropped
    // Every Part of the design for which `of` finds its device, in netlist order.
    template <class Of>
    std::vector<std::string> referencesOf(Of of) const;

    const SessionBuild* build_;
    std::set<std::string, std::less<>> pressed_;                // Buttons held down
    std::set<std::pair<std::string, std::size_t>> matrixHeld_;  // key matrix keys held
    std::map<std::string, double, std::less<>> knobs_;          // PADDLE turns, 0..1
    std::map<std::string, std::string, std::less<>> typed_;     // codes sent, per KEYBOARD
};

}  // namespace bw
